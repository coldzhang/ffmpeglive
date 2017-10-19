#pragma once

#include "XDataThread.h"
#include "XFilter.h"
#include <vector>

//继承了数据线程类
class XVideoCapture : public XDataThread
{
public:
	int width = 0;
	int height = 0;
	int fps = 0;

	static XVideoCapture *Get(unsigned char index = 0);
	virtual bool Init(int camIndex = 0) = 0;
	virtual bool Init(const char* url) = 0;
	virtual void Stop() = 0;

	virtual ~XVideoCapture();

	//添加过滤器
	void AddFilter(XFilter* f)
	{
		fmutex.lock();
		filters.push_back(f);
		fmutex.unlock();
	}
protected:
	QMutex fmutex;

	std::vector <XFilter*> filters;

	XVideoCapture();
};

