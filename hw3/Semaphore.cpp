// Spencer Rawls
// Semaphore.cpp

#include "stdafx.h"
#include "Semaphore.h"

Semaphore::Semaphore(int initValue)
: value(initValue) {}

int Semaphore::Grab()
{
	std::unique_lock<std::mutex> lck(mut);
	while (value == 0)
	{
		cv.wait(lck);
	}
	--value;
	return value;
}

int Semaphore::Release()
{
	std::unique_lock<std::mutex> lck(mut);
	++value;
	cv.notify_one();
	return value;
}