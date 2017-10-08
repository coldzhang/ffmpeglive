/*
使用ffmpeg的播放命令
ffplay "rtmp://192.168.124.142/live live=1"
*/


#include <opencv2/highgui.hpp>
#include<iostream>

#include "XMediaEncode.h"
#include "XRtmp.h"

using namespace std;
using namespace cv;

#pragma comment(lib, "opencv_world320d.lib")


//将海康相机的rtsp数据流转换为rtmp进行推流
int main(int argc, char* argv[])
{
	//海康相机的rtsp url
	char *inUrl = "rtsp://test:test123456@192.168.1.109";
	//nginx-rtmp 直播服务器rtmp推流URL
	char *outUrl = "rtmp://192.168.124.142/live";
	
	//编码器和像素格式转换
	XMediaEncode* me = XMediaEncode::Get(0);
	
	//封装和推流对象
	XRtmp *xr = XRtmp::Get(0);

	VideoCapture cam;
	Mat frame;
	namedWindow("video"); 

	int ret = 0;
	try
	{
		////////////////////////////////
		///1 使用opencv打开rtsp相机
		//cam.open(inUrl);
		cam.open(0);//传入0表示打开任意摄像头，内部匹配
		if (!cam.isOpened())
		{
			throw exception("cam open failed!");
		}
		cout << inUrl << "cam open success" << endl;
		
		int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);//获取到相机的宽
		int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);////获取到相机的高
		int fps = 25;//cam.get(CAP_PROP_FPS);//直接打开摄像头，获取的帧率为0，这里是为了测试强制写成了25

		//2 初始化格式转换上下文
		//3 初始化输出的数据结构
		me->inWidth = inWidth;
		me->inHeight = inHeight;
		me->outWidth = inWidth;
		me->outHeight = inHeight;
	
		me->InitScale();

		/// 4.初始化编码上下文
		//a 找到编码器
		if (!me->InitVideoCodec())
		{
			throw exception("InitVideoCodec failed");
		}
		
		///5 输出封装器和视频流配置
		xr->Init(outUrl);

		//添加视频流
		xr->AddStream(me->vc);
		xr->SendHead();

		for (;;)
		{
			///读取rtsp视频帧，解码视频帧
			if (!cam.grab())//只是做解码，不做图像转换
			{
				continue;
			}
			///转换yuv到rgb
			if (!cam.retrieve(frame))
			{
				continue;
			}

			imshow("video", frame);
			waitKey(1);

			//rgb to yuv
			me->inPixSize = frame.elemSize();
			AVFrame* yuv = me->RGBToYUV((char*)frame.data);
			if (!yuv)
			{
				continue;
			}
			//cout << h << " " << flush;

			///h264编码
			AVPacket *pack = me->EncodeVideo(yuv);
			if (!pack) continue;

			xr->SendFrame(pack);
		}
	}
	catch (exception &ex)
	{
		if (cam.isOpened()) 
				cam.release();
		
		/*if (vsc)
		{
			sws_freeContext(vsc);
			vsc = NULL;
		}*/
		//if (vc)
		//{
		//	/*
		//	不能使用avio_close关闭IO，因为在avcodec_free_context又会关闭一次，造成多次关闭
		//	因此使用avio_closep进行内部置null操作或者直接ic->pb=NULL;
		//	*/
		//	avio_closep(&ic->pb);
		//	avcodec_free_context(&vc);//释放编码器上下文，内部同时会关闭编码器
		//}
		cerr << ex.what() << endl;
	}
	
	getchar();
	return 0;
}