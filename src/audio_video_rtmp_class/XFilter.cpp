#include "XFilter.h"
#include "XBileteralFilter.h"
#include <iostream>

using namespace std;

bool XFilter::Set(std::string key, double value)
{
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
	case XBILATERAL://Ë«±ßÂË²¨
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
