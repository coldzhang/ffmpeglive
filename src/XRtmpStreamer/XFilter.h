#pragma once

enum XFilterType
{
	XBILATERAL//双边滤波
};

//不引用头文件，直接声明该类
namespace cv
{
	class Mat;
}

#include <string>
#include <map>

class XFilter
{
public:
	static XFilter* Get(XFilterType t = XBILATERAL);
	virtual bool Filter(cv::Mat *src, cv::Mat* des) = 0;//滤波器处理函数，需要子类重写，每个滤波器需要使用
	virtual bool Set(std::string key, double value);

	virtual ~XFilter();

protected:
	std::map<std::string, double> paras;//映射

	XFilter();
};

