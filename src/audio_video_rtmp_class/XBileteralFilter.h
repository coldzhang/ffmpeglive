#pragma once
#include "XFilter.h"
class XBileteralFilter :
	public XFilter
{
public:
	XBileteralFilter();
	bool Filter(cv::Mat* src, cv::Mat *des);

	virtual ~XBileteralFilter();
};

