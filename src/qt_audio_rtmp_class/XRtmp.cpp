#include "XRtmp.h"
#include <iostream>
#include <string>

using namespace std;

extern "C"
{
	#include <libavformat/avformat.h>
	#include <libavutil/time.h>
}

#pragma comment(lib, "avformat.lib")

class CXRtmp : public XRtmp
{
public:
	void Close()
	{
		if (ic)
		{
			avformat_close_input(&ic);
			vs = NULL;
		}

		vc = NULL;
		url = "";
	}

	bool Init(const char *url)
	{
		///5 输出封装器和视频流配置 
		//a 创建输出封装器上下文
		int ret = avformat_alloc_output_context2(&ic, 0, "flv", url);
		this->url = url;

		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			cout << buf;

			return false;
		}

		return true;
	}

	bool AddStream(const AVCodecContext *c)
	{
		if (!c) return false;
		
		//b 添加视频流, 新流
		AVStream *st = avformat_new_stream(ic, NULL);
		if (!st)
		{
			cout<<"avformat_new_stream failed!"<<endl;
			return false;
		}

		st->codecpar->codec_tag = 0;//首先设置为0，避免出错
		//从编码器复制参数
		avcodec_parameters_from_context(st->codecpar, c);

		av_dump_format(ic, 0, url.c_str(), 1);

		if (c->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			vc = c;
			vs = st;
		}
		else if (c->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			ac = c;
			as = st;
		}

		return true;
	}

	bool SendHead()
	{
		///打开rtmp的网络输出IO
		int ret = avio_open(&ic->pb, url.c_str(), AVIO_FLAG_WRITE);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			cout << buf << endl;

			return false;
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
			cout << buf << endl;

			return false;
		}

		return true;
	}

	bool SendFrame(AVPacket *pack)
	{
		if (!pack || pack->size <= 0 || !pack->data) return false;
		AVRational stime;
		AVRational dtime;

		//判断音频还是视频
		if (vs && vc && pack->stream_index == vs->index)
		{
			stime = vc->time_base;
			dtime = vs->time_base;
		}
		else if (as && ac && pack->stream_index == as->index)
		{
			stime = ac->time_base;
			dtime = as->time_base;
		}
		else
		{
			return false;
		}
		//推流
		/*
		编码和推流的timebase会有变化，编码是使用帧率进行计算的。推流时要改为使用推流时的timebase重新进行计算
		*/
		pack->pts = av_rescale_q(pack->pts, stime, dtime);
		pack->dts = av_rescale_q(pack->dts, stime, dtime);
		pack->duration = av_rescale_q(pack->duration, stime, dtime);
		/*这个函数内部有缓冲排序，会进行P帧B帧排序，同时还会自动释放packet的空间，不管成功与否
		相对于av_write_frame
		*/
		int ret = av_interleaved_write_frame(ic, pack);
		if (ret == 0)
		{
			cout << "#" << flush;
			return true;
		}
	}
private:
	//rtmp flv封装器
	AVFormatContext *ic = NULL;

	//视频编码器
	const AVCodecContext *vc = NULL;

	//音频编码器
	const AVCodecContext *ac = NULL;

	//视频流
	AVStream *vs = NULL;
	//音频流
	AVStream *as = NULL;

	string url = "";
};

//工厂生产方法
XRtmp *XRtmp::Get(unsigned char index)
{
	static CXRtmp cxr[255];
	static bool isFirst = true;
	if (isFirst)
	{
		//注册所有的封装器
		av_register_all();

		//注册所有的网络协议
		avformat_network_init();

		isFirst = false;
	}

	return &cxr[index];
}

XRtmp::XRtmp()
{
}


XRtmp::~XRtmp()
{
}
