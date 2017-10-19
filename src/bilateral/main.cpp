#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#pragma comment(lib, "opencv_world320d.lib")
using namespace std;
using namespace cv;

/*
opencv双边滤波算法测试
*/
int main(int argc, char argv[])
{
	Mat src = imread("001.jpg");//读取整张图像数据
	if (!src.data)
	{
		cout << "open file failed!" << endl;
		getchar();
		return -1;
	}

	namedWindow("src");
	moveWindow("src", 100, 100);
	imshow("src", src);//显示一张图像数据

	Mat image;
	int d = 3;

	namedWindow("image");
	moveWindow("image", 600, 100);

	for (;;)
	{
		long long b = getTickCount();

		/*
		滤波器是图像处理和计算机视觉中最基础的运算。而Bilateral Filter又是十分经典的一种滤波器，
		这主要得益于它的一个突出的特点，就是对图像进行平滑时，能进行边缘保护。
		而Bilateral Flter的这个特性主要是因为他在平滑滤波时同时考虑了像素间的几何距离和色彩距离。
		对图像进行滤波就是一个加权平均的运算过程，滤波后图像中的每个像素点都是由其原图像中该点临域内多个像素点值的加权平均。
		不同的滤波器，最根本的差异就是权值不同。
		
		双边滤波中双边的意思是同时考虑两条边（因素），这两条边分别是空间域和值域。这里的空间域是指考虑空间位置关系，
		根据距离核心位置的距离的远近给予不同的加权值，原理和高斯滤波一样。而值域是指考虑邻域范围内的像素差值计算出滤波器系数，
		类似于α-截尾均值滤波器（去掉百分率为α的最小值和最大之后剩下像素的均值作为滤波器）。
		双边滤波是结合图像的空间邻近度和像素值相似度的一种折中处理，同时考虑空域信息和灰度相似性，达到保边去燥的目的，具有简单、非迭代、局部的特点。
		双边滤波在图像处理领域中有着广泛的应用，比如去噪、去马赛克、光流估计等等。

		双边滤波函数  保护边缘的平滑滤波器  
		第一个参数为源图像，第二个参数为目标图像
		其中sigmaColor越大，就表明该像素邻域内有越宽广的颜色会被混合到一起，产生较大的相等颜色区域。sigmaSpace越大，
		则表明越远的像素会对kernel中心的像素产生影响，从而使更大的区域中足够相似的颜色获取相同的颜色
		*/
		bilateralFilter(src, image, d, d*2, d/2);

		/*
		GetTickcount函数：它返回从操作系统启动到当前所经的计时周期数
		getTickFrequency函数：返回每秒的计时周期数
		*/
		double sec = (double)(getTickCount() - b)/(double)getTickFrequency();
		cout << "d = "<< d <<"sec is " << sec << endl;

		imshow("image", image);//显示一张图像数据

		int k = waitKey(0);//无限等待键盘输入
		if (k == 'd')
		{
			d += 2;
		}
		else if (k == 'f')
		{
			d -= 2;
		}
		else
		{
			break;
		}
	}
	
	waitKey(0);

	return 0;
}