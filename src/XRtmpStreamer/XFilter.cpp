#include "XFilter.h"
#include "XBileteralFilter.h"
#include <iostream>

using namespace std;

bool XFilter::Set(std::string key, double value)
{
	//首先搜索映射表中是否有某个键，只有存在这个键才进行赋值
	if (paras.find(key) == paras.end())
	{
		cout << "para" << key << " is not support!" << endl;
		return false;
	}

	paras[key] = value;
	return true;
}

XFilter* XFilter::Get(XFilterType t)
{
	static XBileteralFilter xbf;

	switch (t)
	{
	case XBILATERAL://双边滤波
		return &xbf;
		break;
	default:
		break;
	}

	return 0;
}

XFilter::XFilter()
{
}


XFilter::~XFilter()
{
}
