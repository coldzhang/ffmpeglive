#include <opencv2/highgui.hpp>
#include <string>
#include <iostream>

using namespace cv;
using namespace std;

#pragma comment(lib, "opencv_world320d.lib")

/*
opencv播放rtsp海康摄像头和播放系统摄像头
*/
int main()
{
	VideoCapture cam;
	string url = "rtsp://test:test123456@192.168.1.64";//海康摄像机
	namedWindow("video");

	//if (cam.open(url))
	if (cam.open(0))//传入参数为0，可以内部识别摄像机，可以打开usb插入的任意摄像头
	{
		cout << "open cam success!" << endl;
	}
	else
	{
		cout << "open cam failed!" << endl;
		waitKey(0);
		return -1;
	}

	Mat frame;
	for (;;)
	{
		cam.read(frame);//read函数已经进行了解码并图像转换
		imshow("video", frame);//显示图像
		waitKey(1);
	}
	
	getchar();

	return 0;
}