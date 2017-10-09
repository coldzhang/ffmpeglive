#include <QtCore/QCoreApplication>
#include <iostream>
#include <QThread>
#include "XMediaEncode.h"
#include "XRtmp.h"
#include "XAudioRecord.h"

using namespace std;

/*
使用QT音频录制接口进行录制
*/
int main(int argc, char *argv[])
{
	QCoreApplication a(argc, argv);

	//nginx-rtmp 直播服务器rtmp推流URL
	char *outUrl = "rtmp://192.168.124.146/live";

	int ret = 0;

	int sampleRate = 44100;
	int channels = 2;
	int sampleByte = 2;
	int nbSample = 1024;

	//1 qt音频开始录制
	XAudioRecord *ar= XAudioRecord::Get();
	ar->sampleRate = sampleRate;
	ar->channels = channels;
	ar->sampleByte = sampleByte;
	ar->nbSamples = nbSample;
	if (!ar->Init())
	{
		cout << "XAudioRecord Init failed!" << endl;
		getchar();
		return -1;
	}

	/*
	qt录制出来的音频采样点格式是UnSignedInt，但是aac编码需要的格式是float类型，因此需要重采样
	*/
	///2 音频重采样 上下文初始化 
	XMediaEncode* xe = XMediaEncode::Get();
	xe->channels = channels;
	xe->nbSample = nbSample;
	xe->sampleRate = sampleRate;
	xe->inSampleFmt = XSampleFMT::X_S16;
	xe->outSampleFmt = XSampleFMT::X_FLATP;
	if (!xe->InitResample())
	{
		getchar();
		return -1;
	}
 
	///4 初始化音频编码器
	if (!xe->InitAudioCode())
	{
		getchar();
		return -1;
	}

	///5. 输出封装器和音频流配置
	//a 创建输出封装器上下文
	XRtmp *xr = XRtmp::Get(0);
	if (!xr->Init(outUrl))
	{
		getchar();
		return -1;
	}

	//b添加音频流
	if (!xr->AddStream(xe->ac))
	{
		getchar();
		return -1;
	}

	//打开rtmp的网络数据IO
	//写入封装头
	if (!xr->SendHead())
	{
		getchar();
		return -1;
	}

	//一次读取一帧音频的字节数
	int readSize = xe->nbSample * channels * sampleByte;
	for (;;)
	{
		//一次读取一帧音频
		XData d = ar->Pop();
		if (d.size <= 0)
		{
			QThread::msleep(1);
			continue;
		}

		//已经读一帧源数据
		//重采样源数据
		AVFrame* pcm = xe->Resample(d.data);
		d.Drop();

		//pts运算
		//nb_sample/sample_rate = 一帧音频的秒数sec
		//timebase pts = sec * timebase.den
		AVPacket *pkt = xe->EncodeAudio(pcm);
		if (!pkt) continue;

		//推流
		xr->SendFrame(pkt);
		if (ret == 0)
		{
			cout << "#" << flush;
		}
	}

	getchar();
	return a.exec();
}
