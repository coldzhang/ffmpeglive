#pragma once

#include <QThread>
#include <string>
#include "XDataThread.h"

//继承线程类
class XController : public XDataThread
{
public:
	std::string outUrl;//输出的推流地址
	int camIndex = -1;//记录摄像头的打开id
	std::string inUrl = "";//rtsp的流地址
	std::string err = "";

	//使用单例模式，每次获取只能返回同一个对象
	static XController *Get()//必须是静态方法和成员
	{
		static XController xc;
		return &xc;
	}

	//设定美颜参数
	virtual bool Set(std::string key, double val);
	//
	virtual bool Start();
	virtual void Stop();
	void run();//需要重写线程run方法
	virtual ~XController();

protected:
	int vindex = 0;//视频流索引
	int aindex = 1;//音频流索引
	XController();
};

