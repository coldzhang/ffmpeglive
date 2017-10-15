#include "XMediaEncode.h"

extern "C"
{
#include <libswscale/swscale.h>
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include <libswresample/swresample.h>
}

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")

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

		if (asc)
		{
			swr_free(&asc);
		}

		if (yuv)
		{
			av_frame_free(&yuv);//此函数不仅会释放数据内存，还会内部将yuv置null
		}

		if (vc)
		{
			avcodec_free_context(&vc);//释放编码器上下文，内部同时会关闭编码器
		}

		if (pcm)
		{
			av_frame_free(&pcm);//此函数不仅会释放数据内存，还会内部将yuv置null
		}


		vpts = 0;
		av_packet_unref(&vpack);
		apts = 0;
		av_packet_unref(&apack);
	}

	bool InitAudioCode()
	{
		///4 初始化音频编码器
		if (!(ac = CreateCodec(AV_CODEC_ID_AAC)))
		{
			return false;
		}

		ac->bit_rate = 40000;
		ac->sample_rate = sampleRate;
		ac->sample_fmt = AV_SAMPLE_FMT_FLTP;
		ac->channels = channels;
		ac->channel_layout = av_get_default_channel_layout(channels);

		//打开音频编码器
		return OpenCodec(&ac);
	}

	bool InitVideoCodec()
	{
		/// 4.初始化编码上下文
		//a 找到编码器
		if (!(vc = CreateCodec(AV_CODEC_ID_H264)))
		{
			return false;
		}

		vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
		vc->width = outWidth;
		vc->height = outHeight;
		//vc->time_base = { 1, fps };//时间戳基数，其实就是每一帧占用的时长
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
		return OpenCodec(&vc);;
	}

	//视频编码
	XData EncodeVideo(XData frame)
	{
		av_packet_unref(&vpack);//先清理上次packet的数据

		XData r;
		if (frame.size <= 0 || !frame.data) return r;
		AVFrame* p = (AVFrame*)frame.data;

		///h264编码
		//frame->pts = vpts;
		//vpts++;
		/*
		编码分两个步骤avcodec_send_frame和avcodec_receive_packet
		avcodec_send_frame只是单线程简单的将数据拷贝过去，而avcodec_receive_packet是会
		内部开多个线程进行编码然后返回编码后的数据
		*/

		/*
		avcodec_send_frame内部是会有缓冲，因此不一定调用了avcodec_send_frame，就会avcodec_receive_packet获取到数据，
		若将第二个参数传null，可以获取到缓冲区内的数据
		*/
		int ret = avcodec_send_frame(vc, p);
		if (ret != 0)
		{
			return r;
		}

		/*
		调用avcodec_receive_packet时，第二个参数每次都会内部先调用av_frame_unref进行清掉
		*/
		ret = avcodec_receive_packet(vc, &vpack);
		if (ret != 0 || vpack.size <= 0)
		{
			return r;
		}

		r.data = (char*)&vpack;
		r.size = vpack.size;
		r.pts = frame.pts;

		return r;
	}

	long long lasta = -1;
	XData EncodeAudio(XData frame)
	{
		XData r;

		//pts运算
		//nb_sample/sample_rate = 一帧音频的秒数sec
		//timebase pts = sec * timebase.den
		if (frame.size <= 0 || !frame.data) return r;

		AVFrame* p = (AVFrame* )frame.data;
		if (lasta == p->pts)//前后时间戳不能相等，若相等，可以将当前时间戳增加一点
		{
			p->pts += 1200;
		}
		lasta = p->pts;

		int ret = avcodec_send_frame(ac, p);
		if (ret != 0) return r;

		av_packet_unref(&apack);
		ret = avcodec_receive_packet(ac, &apack);
		if (ret != 0) return r;

		cout << apack.size << " " << flush;
		r.data = (char*)&apack;
		r.size = apack.size;
		r.pts = frame.pts;

		return r;
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

	XData RGBToYUV(XData d)
	{
		XData r;
		r.pts = d.pts;

		//rgb to yuv
		//输入的数据结构
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		//indata[0]  bgrbgrbgr
		//plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr
		indata[0] = (uint8_t*)d.data;
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
			return r;
		}

		yuv->pts = d.pts;

		r.data = (char*)yuv;
		int *p = yuv->linesize;
		while (*p)//累加得到yuv图像的总大小
		{
			r.size += (*p)*outHeight;
			p++;
		}

		return r;
	}

	bool InitResample()
	{
		/*
		qt录制出来的音频采样点格式是UnSignedInt，但是aac编码需要的格式是float类型，因此需要重采样
		*/
		///2 音频重采样 上下文初始化 
		asc = swr_alloc_set_opts(asc,
			av_get_default_channel_layout(channels), (AVSampleFormat)outSampleFmt, sampleRate,//输出格式
			av_get_default_channel_layout(channels), (AVSampleFormat)inSampleFmt, sampleRate, 0, 0); //输入格式																	//输入格式
		if (!asc)
		{
			cout << "swr_alloc_set_opts failed!" << endl;
			return false;
		}

		int ret = swr_init(asc);
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << endl;

			return false;
		}

		cout << "音频重采样上下文初始化成功" << endl;

		///3 音频重采样输出空间分配
		pcm = av_frame_alloc();
		pcm->format = outSampleFmt;
		pcm->channels = channels;
		pcm->channel_layout = av_get_default_channel_layout(channels);
		pcm->nb_samples = nbSample; //一帧音频一通道的采样点数，内部然后可以通过这个值及通道数，及采样点的大小计算一秒的音频字节数及一帧音频的字节数
		ret = av_frame_get_buffer(pcm, 0);//给pcm分配存储空间，第二个参数传0表示不需要对齐
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << endl;

			return false;
		}

		return true;
	}

	XData Resample(XData d)
	{
		XData r;

		//已经读一帧源数据
		//重采样源数据
		const uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t *)d.data;
		//返回重采样后每一个声道的采样点数
		int len = swr_convert(asc, pcm->data, pcm->nb_samples,//输出参数，输出存储地址和样本数量
			indata, pcm->nb_samples);

		if (len <= 0)
		{
			return r;
		}

		pcm->pts = d.pts;

		r.data = (char*)pcm;
		r.size = pcm->nb_samples*pcm->channels * 2;
		r.pts = d.pts;

		return r;
	}
private:
	AVCodecContext* CreateCodec(AVCodecID cid) 
	{
		///4 初始化音频编码器
		AVCodec *codec = avcodec_find_encoder(cid);
		if (!codec)
		{
			cout << "avcodec_find__encoder failed!" << endl;
			return NULL;
		}

		//音频编码器上下文
		AVCodecContext* c = avcodec_alloc_context3(codec);
		if (!c)
		{
			cout << "avcodec_alloc_context3 failed!" << endl;
			return NULL;
		}

		cout << "avcodec_alloc_context3 success!" << endl;

		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		c->thread_count = XGetCpuNum();//设置了编码器的内部编码线程数量
		c->time_base = { 1, 1000000 };//设置时间戳基准

		return c;
	}

	bool OpenCodec(AVCodecContext **c)
	{
		//打开音频编码器
		int ret = avcodec_open2(*c, 0, 0);
		if (ret != 0)
		{
			char err[1024] = { 0 };
			av_strerror(ret, err, sizeof(err) - 1);
			cout << err << endl;
			avcodec_free_context(c);
			return false;
		}
		cout << "avcodec_open2 success!" << endl;
	}

	SwsContext *vsc = NULL;//像素格式转换上下文
	AVFrame *yuv = NULL;//输出的YUV
	//AVCodecContext *vc = NULL;//编码器上下文
	AVPacket vpack = {0};//视频帧
	AVPacket apack = {0};//音频帧
	int vpts = 0;
	int apts = 0;
	SwrContext* asc = NULL;//音频重采样上下文
	AVFrame *pcm = NULL;//重采样输出的pcm
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
