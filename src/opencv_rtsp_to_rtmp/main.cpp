#include <opencv2/highgui.hpp>
#include<iostream>
extern "C"
{
	#include <libswscale/swscale.h>
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
}

using namespace std;
using namespace cv;

#pragma comment(lib, "opencv_world320d.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")

//将海康相机的rtsp数据流转换为rtmp进行推流
int main(int argc, char* argv[])
{
	//海康相机的rtsp url
	char *inUrl = "rtsp://test:test123456@192.168.1.109";
	//nginx-rtmp 直播服务器rtmp推流URL
	char *outUrl = "rtmp://192.168.124.139/live";
	
	/*
	注册所有的编解码器,注意区分
	av_register_all();初始化所有封装和解封装 如flv mp4 mov mp3,不包含编码器和解码器
	*/
	avcodec_register_all();

	//注册所有的封装器
	av_register_all();

	//注册所有的网络协议
	avformat_network_init();

	VideoCapture cam;
	Mat frame;
	namedWindow("video"); 

	//像素格式转换上下文
	SwsContext *vsc = NULL;
	AVFrame *yuv = NULL;

	//编码器上下文
	AVCodecContext *vc = NULL;

	//rtmp flv封装器
	AVFormatContext *ic = NULL;

	try
	{
		////////////////////////////////
		///1 使用opencv打开rtsp相机
		//cam.open(inUrl);
		cam.open(0);//传入0表示打开任意摄像头，内部匹配
		if (!cam.isOpened())
		{
			throw exception("cam open failed!");
		}
		cout << inUrl << "cam open success" << endl;
		
		int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);//获取到相机的宽
		int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);////获取到相机的高
		int fps = 25;//cam.get(CAP_PROP_FPS);//直接打开摄像头，获取的帧率为0，这里是为了测试强制写成了25

		//2 初始化格式转换上下文  此函数可以根据尺寸的变化进行内部重新创建上下文
		vsc = sws_getCachedContext(vsc,
			inWidth, inHeight, AV_PIX_FMT_BGR24,//源宽，高，像素格式
			inWidth, inHeight, AV_PIX_FMT_YUV420P,//目标宽，高，像素格式
			SWS_BICUBIC,//三线性插值， 尺寸变化使用算法，若尺寸不变化，则不使用
			0, 0, 0//过滤器等参数不使用
		);
		if (!vsc)
		{
			throw exception("sws_getCachedContext failed!");
		}

		//3.初始化输出的数据结构
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = inWidth;
		yuv->height = inHeight;
		yuv->pts = 0;
		//分配yuv空间，并且是32位对齐的
		int ret = av_frame_get_buffer(yuv, 32);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		/// 4.初始化编码上下文
		//a 找到编码器
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);//并不需要释放指针内存，因为仅是取得内存地址而已
		if (!codec)
		{
			throw exception("Can not find h264 encoder!");
		}
		//b 创建编码器上下文
		vc = avcodec_alloc_context3(codec);
		if (!vc)
		{
			throw exception("avcodec_alloc_context3 failed!");
		}
		//c 配置编码器参数
		vc->flags = AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
		vc->codec_id = codec->id;
		vc->thread_count = 8;//编码的线程数量
		vc->bit_rate = 50*1024*8;//压缩后每秒视频的bit位大小 50kB
		vc->width = inWidth;
		vc->height = inHeight;
		vc->time_base = {1, fps};//时间戳基数，其实就是每一帧占用的时长
		vc->framerate = {fps, 1};//帧率

		/*画面组的大小，多少帧一个关键帧
		关键帧越大，则压缩率越高，因为后面的帧比如P帧，B帧都是保存与前帧或后帧的变化数据。但是带来的负面影响
		就是比如得到第50帧，那么得将前面49帧全部解码才能显示第50帧
		*/
		vc->gop_size = 50;
		/*
		设置B帧数量为0，表示不编码B帧，这样pts和dts则一致
		因为有B帧的话，需要先解码后面的P帧，B帧是根据前后帧进行压缩的图像帧，压缩率很高
		*/
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;

		//d 打开编码器上下文
		/*
		第一个参数是编码器上下文，第二个参数是编码器，若在avcodec_alloc_context3调用时设置了，则这里可以
		不指定，设为0，第三个参数设置也默认设为0
		*/
		ret = avcodec_open2(vc, 0, 0);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		cout << "avcodec_open2 success!" << endl;

		///5 输出封装器和视频流配置 
		//a 创建输出封装器上下文
		ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		//b 添加视频流, 新流
		AVStream *vs = avformat_new_stream(ic, NULL);
		if (!vs)
		{
			throw exception("avformat_new_stream failed!");
		}
		vs->codecpar->codec_tag = 0;//首先设置为0，避免出错
		//从编码器复制参数
		avcodec_parameters_from_context(vs->codecpar, vc);
		
		av_dump_format(ic, 0, outUrl, 1);

		///打开rtmp的网络输出IO
		ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		/*写入封装头 这一步可能会将编码器AVStream中的timebase给改掉，
		因此后续使用时,timebase需要重新从AVStream中获取,使用改后的timebase计算才是正确的，
		不能使用之前设置的timebase
		*/
		ret = avformat_write_header(ic, NULL);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		AVPacket pack;
		memset(&pack, 0, sizeof(pack));
		int vpts = 0;
		for (;;)
		{
			///读取rtsp视频帧，解码视频帧
			if (!cam.grab())//只是做解码，不做图像转换
			{
				continue;
			}
			///转换yuv到rgb
			if (!cam.retrieve(frame))
			{
				continue;
			}

			imshow("video", frame);
			waitKey(1);

			//rgb to yuv
			//输入的数据结构
			uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
			//indata[0]  bgrbgrbgr
			//plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr
			indata[0] = frame.data;
			int insize[AV_NUM_DATA_POINTERS] = { 0 };
			//一行(宽)数据的字节数
			insize[0] = frame.cols * frame.elemSize();
			
			/*
			格式转换函数
			第一个参数为格式转换上下文
			第二个参数为输入数据，第三个参数为输入图像一行的字节数，第四个参数为图像数据的起始
			位置，默认为0表示从头开始，第五个参数输入数据的高
			返回值为转换后图像的高度
			*/
			int h = sws_scale(vsc, indata, insize, 0, frame.rows, //源数据
			yuv->data, yuv->linesize);//目标数据的buffer和目标图像一行的字节数
			if (h <= 0)
			{
				continue;
			}
			//cout << h << " " << flush;

			///h264编码
			yuv->pts = vpts;
			vpts++;
			/*
			编码分两个步骤avcodec_send_frame和avcodec_receive_packet
			avcodec_send_frame只是单线程简单的将数据拷贝过去，而avcodec_receive_packet是会
			内部开多个线程进行编码然后返回编码后的数据
			*/

			/*
			avcodec_send_frame内部是会有缓冲，因此不一定调用了avcodec_send_frame，就会avcodec_receive_packet获取到数据，
			若将第二个参数传null，可以获取到缓冲区内的数据
			*/
			ret = avcodec_send_frame(vc, yuv);
			if (ret != 0)
			{
				continue;
			}
			
			/*
			调用avcodec_receive_packet时，第二个参数每次都会内部先调用av_frame_unref进行清掉
			*/
			ret = avcodec_receive_packet(vc, &pack);
			if (ret != 0 || pack.size > 0)
			{
				cout << "*" << pack.size << flush;
			}
			else
			{
				continue;
			}


			//推流
			/*
			编码和推流的timebase会有变化，编码是使用帧率进行计算的。推流时要改为使用推流时的timebase重新进行计算
			*/
			pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
			pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
			/*这个函数内部有缓冲排序，会进行P帧B帧排序，同时还会自动释放packet的空间，不管成功与否
			相对于av_write_frame
			*/
			ret = av_interleaved_write_frame(ic, &pack);
			if (ret == 0)
			{
				cout << "#" << flush;
			}
		}
	}
	catch (exception &ex)
	{
		if (cam.isOpened()) 
				cam.release();
		if (vsc)
		{
			sws_freeContext(vsc);
			vsc = NULL;
		}
		if (vc)
		{
			/*
			不能使用avio_close关闭IO，因为在avcodec_free_context又会关闭一次，造成多次关闭
			因此使用avio_closep进行内部置null操作或者直接ic->pb=NULL;
			*/
			avio_closep(&ic->pb);
			avcodec_free_context(&vc);//释放编码器上下文，内部同时会关闭编码器
		}
		cerr << ex.what() << endl;
	}
	
	getchar();
	return 0;
}