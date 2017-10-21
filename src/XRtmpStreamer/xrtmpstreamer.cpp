#include "xrtmpstreamer.h"
#include <iostream>
#include "XController.h"

using namespace std;

static bool isStream = false;

XRtmpStreamer::XRtmpStreamer(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
}

//开始按钮点击信号的响应槽函数
void XRtmpStreamer::Stream()
{
	cout << "Stream" << endl;
	if (isStream)
	{
		isStream = false;
		ui.pushButton->setText(QString::fromLocal8Bit("开始"));

		XController::Get()->Stop();
	}
	else
	{
		isStream = true;
		ui.pushButton->setText(QString::fromLocal8Bit("停止"));
		QString url = ui.inUrl->text();
		bool ok = false;
		int camIndex = url.toInt(&ok);
		if (!ok)
		{
			XController::Get()->inUrl = url.toStdString();//设置打开摄像头的方式
		}
		else
		{
			XController::Get()->camIndex = camIndex;
		}
		
		XController::Get()->outUrl = ui.outUrl->text().toStdString();//设置推流地址
		XController::Get()->Set("d", (ui.face->currentIndex() + 1) * 3);//设置美颜级别

		XController::Get()->Start();//开始推流
	}
}