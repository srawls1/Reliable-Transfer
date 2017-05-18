// Spencer Rawls
// Semaphore.h

#pragma once
#include <mutex>
#include <condition_variable>

class Semaphore
{
	std::mutex mut;
	std::condition_variable cv;
	int value;
public:
	Semaphore(int initValue = 0);

	int Grab();
	int Release();
};