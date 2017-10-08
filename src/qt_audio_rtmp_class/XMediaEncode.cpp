#include "XMediaEncode.h"

extern "C"
{
#include <libswscale/swscale.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
}

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")

#include <iostream>
using namespace std;

//获取CPU的数量
//WIN32 32位的宏
#if defined WIN32 || defined _WIN32//64位的宏
#include <windows.h>
#endif
static int XGetCpuNum()
{
#if defined WIN32 || defined _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return (int)sysinfo.dwNumberOfProcessors;
#elif defined __linux__
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined __APPLE__
	int numCPU = 0;
	int mib[4];
	size_t len = sizeof(numCPU);

	//set the mib for hw.ncpu
	mib[0] = CTL_HW;
	mib0[1] = HW_AVAILCPU;//alternatively, try HW_NCPU

	sysctl(mib, 2, &numCPU, &len, NULL, 0);

	if (numCPU < 1)
	{
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &numCPU, &len, NULL, 0);

		if (numCPU < 1)
			numCPU = 1;
	}
	return (int)numCPU;
#else
	return 1;
#endif
}

class CXMediaEncode : public XMediaEncode
{
public:
	void Close()
	{
		if (vsc)
		{
			sws_freeContext(vsc);
			vsc = NULL;
		}

		if (yuv)
		{
			av_frame_free(&yuv);//此函数不仅会释放数据内存，还会内部将yuv置null
		}

		if (vc)
		{
			avcodec_free_context(&vc);//释放编码器上下文，内部同时会关闭编码器
		}

		vpts = 0;
		av_packet_unref(&pack);
	}

	bool InitVideoCodec()
	{
		/// 4.初始化编码上下文
		//a 找到编码器
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);//并不需要释放指针内存，因为仅是取得内存地址而已
		if (!codec)
		{
			cout << "Can not find h264 encoder!" << endl;
			return false;
		}
		//b 创建编码器上下文
		vc = avcodec_alloc_context3(codec);
		if (!vc)
		{
			cout << "avcodec_alloc_context3 failed!" << endl;
		}
		//c 配置编码器参数
		vc->flags = AV_CODEC_FLAG_GLOBAL_HEADER;//全局参数
		vc->codec_id = codec->id;
		vc->thread_count = XGetCpuNum();//编码的线程数量

		vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
		vc->width = outWidth;
		vc->height = outHeight;
		vc->time_base = { 1, fps };//时间戳基数，其实就是每一帧占用的时长
		vc->framerate = { fps, 1 };//帧率

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
		int ret = avcodec_open2(vc, 0, 0);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			cout << buf << endl;
			return false;
		}

		cout << "avcodec_open2 success!" << endl;

		return true;
	}

	//视频编码
	AVPacket* EncodeVideo(AVFrame* frame)
	{
		av_packet_unref(&pack);//先清理上次packet的数据

		///h264编码
		frame->pts = vpts;
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
		int ret = avcodec_send_frame(vc, frame);
		if (ret != 0)
		{
			return NULL;
		}

		/*
		调用avcodec_receive_packet时，第二个参数每次都会内部先调用av_frame_unref进行清掉
		*/
		ret = avcodec_receive_packet(vc, &pack);
		if (ret != 0 || pack.size <= 0)
		{
			return NULL;
		}

		return &pack;
	}

	bool InitScale()
	{
		//2 初始化格式转换上下文  此函数可以根据尺寸的变化进行内部重新创建上下文
		vsc = sws_getCachedContext(vsc,
			inWidth, inHeight, AV_PIX_FMT_BGR24,//源宽，高，像素格式
			outWidth, outHeight, AV_PIX_FMT_YUV420P,//目标宽，高，像素格式
			SWS_BICUBIC,//三线性插值， 尺寸变化使用算法，若尺寸不变化，则不使用
			0, 0, 0//过滤器等参数不使用
		);
		if (!vsc)
		{
			cout<<"sws_getCachedContext failed!"<<endl;
			return false;
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

		return true;
	}

	AVFrame* RGBToYUV(char *rgb)
	{
		//rgb to yuv
		//输入的数据结构
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		//indata[0]  bgrbgrbgr
		//plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr
		indata[0] = (uint8_t*)rgb;
		int insize[AV_NUM_DATA_POINTERS] = { 0 };
		//一行(宽)数据的字节数
		insize[0] = inWidth * inPixSize;

		/*
		格式转换函数
		第一个参数为格式转换上下文
		第二个参数为输入数据，第三个参数为输入图像一行的字节数，第四个参数为图像数据的起始
		位置，默认为0表示从头开始，第五个参数输入数据的高
		返回值为转换后图像的高度
		*/
		int h = sws_scale(vsc, indata, insize, 0, inHeight, //源数据
			yuv->data, yuv->linesize);//目标数据的buffer和目标图像一行的字节数
		if (h <= 0)
		{
			return NULL;
		}

		return yuv;
	}

private:
	SwsContext *vsc = NULL;//像素格式转换上下文
	AVFrame *yuv = NULL;//输出的YUV
	//AVCodecContext *vc = NULL;//编码器上下文
	AVPacket pack = {0};
	int vpts = 0;
};

XMediaEncode* XMediaEncode::Get(unsigned char index)
{
	static bool isFirst = true;
	if (isFirst)
	{
		/*
		注册所有的编解码器,注意区分
		av_register_all();初始化所有封装和解封装 如flv mp4 mov mp3,不包含编码器和解码器
		*/
		avcodec_register_all();
		isFirst = false;
	}

	static CXMediaEncode cxm[255];
	return &cxm[index];
}

XMediaEncode::XMediaEncode()
{
}


XMediaEncode::~XMediaEncode()
{
}
