#pragma once

#include "XData.h"

struct AVFrame;//声明类
struct AVPacket;
class AVCodecContext;
enum XSampleFMT
{
	X_S16=1,
	X_FLATP=8
};

//音视频编码接口类
class XMediaEncode
{
public:
	//输入参数
	int inWidth = 640;
	int inHeight = 480;
	int inPixSize = 3;

	int channels = 2;
	int sampleRate = 44100;
	XSampleFMT inSampleFmt = X_S16;

	//输出参数
	int outWidth = 640;
	int outHeight = 480;
	int bitrate = 4000000;//压缩后每秒视频的bit位大小 50kB
	int fps = 25;

	XSampleFMT outSampleFmt = X_FLATP;
	int nbSample = 1024;

	//工厂生产方法
	static XMediaEncode* Get(unsigned char index = 0);

	//初始化像素格式转换的上下文
	virtual bool InitScale() = 0;

	//返回值无需调用者清理
	virtual XData RGBToYUV(XData rgb) = 0;

	//音频重采样上下文初始化
	virtual bool InitResample() = 0;

	//返回值无需调用者清理
	virtual XData Resample(XData d) = 0;

	//视频编码器初始化
	virtual bool InitVideoCodec() = 0;

	//音频编码器初始化
	virtual bool InitAudioCode() = 0;

	//视频编码  返回值无需调用者清理
	virtual XData EncodeVideo(XData frame) = 0;

	//音频编码 返回值无需调用者清理
	virtual XData EncodeAudio(XData frame) = 0;

	virtual ~XMediaEncode();

	AVCodecContext *vc = 0;//视频编码器上下文
	AVCodecContext* ac = 0;//音频编码器上下文
protected:
	//设计成工厂模式，将构造函数放在保护属性中，避免用户创建对象,由内部来创建，实际就是单例模式
	XMediaEncode();
};

