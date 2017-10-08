extern "C"
{
#include "libavformat\avformat.h"//引入头文件 强制编译器使用C方式编译
#include "libavutil\time.h"
}

#include <iostream>
using namespace std;

/*预处理指令， 引用库
  第二种方式就是在项目-属性-链接器-输入-附加依赖项，将库名编辑进去
*/
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")

int XError(int errNum)
{
	char buf[1024] = { 0 };
	av_strerror(errNum, buf, sizeof(buf));
	cout << buf << endl;
	getchar();
	return -1;
}

static double r2d(AVRational r)
{
	return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}

/*
将rtsp协议的数据源转换为rtmp协议进行发送
海康摄像头发送过来的直接就是rtsp协议的压缩码流数据
*/
int main(int argc, char *argv[])
{
	//char *inUrl = "test.flv";
	//这是海康摄像头通过rtsp协议的数据源  
	char *inUrl = "rtsp://test:test123456@192.168.1.64";
	char *outUrl = "rtmp://192.168.124.137/live";

	//初始化所有封装和解封装 如flv mp4 mov mp3,不包含编码器和解码器
	av_register_all();

	//初始化网络库
	avformat_network_init();

	/////////////////////////////////////////////////////////
	//1 打开文件,解封装
	//输入封装上下文
	/*
	AVFormatContext

	AVIOContext *pb;//IO上下文
	AVStream **streams;//视频音频字幕流
	int nb_streams;//流的数量

	AVStream
	AVRational time_base;
	AVCodecParameters *codecpar;(音视频参数)
	AVCodecContext *codec;
	*/
	AVFormatContext *ictx = NULL;
	
	//设置最大延时及传输方式操作，避免丢帧花屏现象
	AVDictionary *opts = NULL;
	char key[] = "max_delay";
	char val[] = "500";//最大延时500ms
	av_dict_set(&opts, key, val, 0);
	char key2[] = "rtsp_transport";
	char val2[] = "tcp";
	av_dict_set(&opts, key2, val2, 0);

	//打开rtsp流， 解封文件头
	int ret = avformat_open_input(&ictx, inUrl, 0, &opts);
	if (ret != 0)
	{
		return XError(ret);
	}

	cout << "open file " << inUrl << " Success." << endl;

	//获取音视频流信息 h264, flv
	ret = avformat_find_stream_info(ictx, 0);
	if (ret != 0)
	{
		return XError(ret);
	}

	//输出avformat的信息，第二个参数为0表示打印所有的流信息，第四个参数表示输入
	av_dump_format(ictx, 0, inUrl, 0);

	///////////////////////////////////////////////////////////
	//创建输出流上下文
	AVFormatContext *octx = NULL;
	ret = avformat_alloc_output_context2(&octx, 0, "flv", outUrl);
	if (!octx)
	{
		return XError(ret);
	}
	cout << "octx create success!" << endl;

	//配置输出流
	//遍历输入的AVStream
	for (int i = 0; i < ictx->nb_streams; i++)
	{
		//创建输出流
		AVStream* out = avformat_new_stream(octx, ictx->streams[i]->codec->codec);
		if (!out)
		{
			return XError(0);
		}

		//注释行 复制配置信息,用于mp4
		//ret = avcodec_copy_context(out->codec, ictx->streams[i]->codec);
		ret = avcodec_parameters_copy(out->codecpar, ictx->streams[i]->codecpar);
		out->codec->codec_tag = 0;//告诉编码器无需编码，直接写文件
	}

	//打印输出格式的信息，最后参数表示输出的格式
	av_dump_format(octx, 0, outUrl, 1);

	///////////////////////////////////////////////
	//rtmp推流

	//打开IO
	ret = avio_open(&octx->pb, outUrl, AVIO_FLAG_WRITE);
	if (!octx->pb)
	{
		return XError(ret);
	}
	//写入头信息
	avformat_write_header(octx, 0);
	if (ret < 0)
	{
		return XError(ret);
	}
	cout << "avformat_write_header " << ret <<endl;

	//推流每一帧数据
	AVPacket pkt;
	long long startTime = av_gettime();//获取当前时间戳
	for (;;)
	{
		ret = av_read_frame(ictx, &pkt);
		if (ret != 0 || pkt.size <= 0)
		{
			continue;
		}
		cout << pkt.pts << " "<< flush;
		//计算转换pts dts  由于输入和输出的timebase可能不一致，因此需要进行转换,将输入的转换为输出来计算，计算方式为最后一个参数
		AVRational itime = ictx->streams[pkt.stream_index]->time_base;
		AVRational otime = octx->streams[pkt.stream_index]->time_base;
		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.pos = -1;
	
		/*
		由于数据源为rtsp数据，则不需要进行延时操作，因为rtsp数据流已经做了延时操作，固定按照帧率发送数据过来,
		av_read_frame会有阻塞操作，等待数据发送
		*/
#if 0   
		//视频帧
		if (ictx->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVRational tb = ictx->streams[pkt.stream_index]->time_base;
			//已经过去的时间
			long long now = av_gettime() - startTime;
			long long dts = 0;
			//用于控制推流速度，使用解码时间戳和当前时间戳进行对比
			dts = pkt.dts * (1000 * 1000 * r2d(tb));
			if (dts > now)
				av_usleep(dts - now);
		}
#endif

		//发送数据, 这个函数相比av_write_frame可以根据缓冲进行自动排序发送，而且还会自动释放packet的内存
		ret = av_interleaved_write_frame(octx, &pkt);
		if (ret < 0)
		{
			//XError(ret);
		}
		//av_packet_unref(&pkt);//释放帧数据内存
	}

	cout << "file to rtmp test" << endl;
	getchar();

	return 0;
}