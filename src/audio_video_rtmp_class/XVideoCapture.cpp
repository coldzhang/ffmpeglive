#include "XVideoCapture.h"
#include <iostream>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

#pragma comment(lib, "opencv_world320d.lib")

class CXVideoCapture : public XVideoCapture
{
public:
	VideoCapture cam;

	void run()
	{
		Mat frame;

		while (!isExit)
		{
			if (!cam.read(frame))
			{
				msleep(1);
				continue;
			}

			if (frame.empty())
			{
				msleep(1);
				continue;
			}

			//确保数据是连续的
			XData d((char*)frame.data, frame.cols*frame.rows*frame.elemSize(), GetCurTime());//每采集一帧，记一次当前时间
			Push(d);
		}
	}

	bool Init(int camIndex)
	{
		////////////////////////////////
		///1 使用opencv打开rtsp相机
		//cam.open(inUrl);
		cam.open(camIndex);//传入0表示打开任意摄像头，内部匹配
		if (!cam.isOpened())
		{
			cout << "cam open failed!" << endl;

			return false;
		}
		cout << camIndex << "cam open success" << endl;

		width = cam.get(CAP_PROP_FRAME_WIDTH);//获取到相机的宽
		height = cam.get(CAP_PROP_FRAME_HEIGHT);////获取到相机的高
		fps = cam.get(CAP_PROP_FPS);//直接打开摄像头，获取的帧率为0，这里是为了测试强制写成了25
		if (fps == 0) fps = 25;

		return true;
	}

	bool Init(const char* url)
	{
		////////////////////////////////
		///1 使用opencv打开rtsp相机
		//cam.open(inUrl);
		cam.open(url);//传入0表示打开任意摄像头，内部匹配
		if (!cam.isOpened())
		{
			cout << "cam open failed!" << endl;

			return false;
		}
		cout << url << "cam open success" << endl;

		width = cam.get(CAP_PROP_FRAME_WIDTH);//获取到相机的宽
		height = cam.get(CAP_PROP_FRAME_HEIGHT);////获取到相机的高
		fps = cam.get(CAP_PROP_FPS);//直接打开摄像头，获取的帧率为0，这里是为了测试强制写成了25
		if (fps == 0) fps = 25;

		return true;
	}

	void Stop()
	{
		XDataThread::Stop();

		if (cam.isOpened())
		{
			cam.release();
		}
	}
};

XVideoCapture *XVideoCapture::Get(unsigned char index)
{
	static CXVideoCapture xc[255];
	return &xc[index];
}

XVideoCapture::XVideoCapture()
{
}


XVideoCapture::~XVideoCapture()
{
}
