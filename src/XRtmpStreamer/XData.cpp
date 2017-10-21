#include "XData.h"
#include <stdlib.h>
#include<string.h>

//全局函数，获取当前时间，使用ffmpeg的获取时间的接口
long long GetCurTime()
{
	return av_gettime();
}

void XData::Drop()
{
	if (data)
		delete data;
	data = 0;
	size = 0;
}

XData::XData(char *data, int size, long long p)
{
	this->data = new char[size];//重新分配空间，将外部数据拷贝到数据包封装类中
	memcpy(this->data, data, size);
	this->size = size;
	this->pts = p;
}

XData::XData()
{
}


XData::~XData()
{
}
