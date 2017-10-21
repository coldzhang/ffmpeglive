#pragma once

enum XFilterType
{
	XBILATERAL//Ë«±ßÂË²¨
};

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
	virtual bool Filter(cv::Mat *src, cv::Mat* des) = 0;
	virtual bool Set(std::string key, double value);

	virtual ~XFilter();

protected:
	std::map<std::string, double> paras;//Ó³Éä

	XFilter();
};

