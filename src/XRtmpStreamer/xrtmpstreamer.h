#pragma once

#include <QtWidgets/QWidget>
#include "ui_xrtmpstreamer.h"

class XRtmpStreamer : public QWidget
{
    Q_OBJECT

public:
    XRtmpStreamer(QWidget *parent = Q_NULLPTR);

	//QT信号的槽函数声明时必须使用public slots:   还有必须有Q_OBJECT
public slots:
	void Stream();
private:
    Ui::XRtmpStreamerClass ui;
};
