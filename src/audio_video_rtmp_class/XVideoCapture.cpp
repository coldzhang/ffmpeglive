#include "XVideoCapture.h"
#include <iostream>
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

#pragma comment(lib, "opencv_world320d.lib")

/*
视频采集类，采用工厂模式
*/
class CXVideoCapture : public XVideoCapture
{
public:
	VideoCapture cam;

	//因为继承了线程类，因此此处重写run方法在子线程中进行数据采集
	void run()
	{
		Mat frame;

		while (!isExit)
		{
			/*
			VideoCapture的read函数是结合了grab和retrieve函数
			grab:捕获下一帧,对于rtsp摄像头，其捕获的是一帧h264码流,因为rtsp网络摄像头内部已经做了编码操作，然后
			网络传输过来的，因此在grab后，需要进行解码操作,解码成yuv图像，然而对于本地的普通摄像头，则是直接捕获的图像数据，摄像头
			硬件内部给过来的是yuv图像数据。
			retrieve:这个函数是内部做了图像转换，将yuv图像转换为rgb格式，因此frame中就是rgb图像格式数据。

			read内部就是同时做了grab和retrieve的工作
			*/
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
