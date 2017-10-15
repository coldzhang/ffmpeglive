#include "XDataThread.h"

//清除链表中所有数据
void XDataThread::Clear()
{
	mutex.lock();
	while (!datas.empty())
	{
		datas.front().Drop();
		datas.pop_front();
	}

	mutex.unlock();
}

//在列表结尾插入
void XDataThread::Push(XData d)
{
	mutex.lock();
	if (datas.size() > maxList)//数据链表中的缓存大于最大值
	{
		datas.front().Drop();//先清除头节点旧数据
		datas.pop_front();//并将整个数据包节点从链表中删除
	}
	datas.push_back(d);//将新数据放入链表的尾节点
	mutex.unlock();
}

//读取列表中最早的数据
XData XDataThread::Pop()
{
	mutex.lock();
	if (datas.empty())//链表中无数据节点，则返回数据包，包中无实际数据
	{
		mutex.unlock();//记住这里需要解锁   
		return XData();
	}
	XData d = datas.front();//返回头节点数据
	datas.pop_front();
	mutex.unlock();

	return d;
}

//启动线程
bool XDataThread::Start()
{
	isExit = false;
	QThread::start();

	return true;
}

//退出线程，并等待线程退出(阻塞)
void XDataThread::Stop()
{
	isExit = true;
	wait();
}

XDataThread::XDataThread()
{
}


XDataThread::~XDataThread()
{
}
