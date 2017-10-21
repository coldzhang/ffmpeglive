#include "xrtmpstreamer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    XRtmpStreamer w;
    w.show();
    return a.exec();
}
