#include "semaphore.h"
#include<iostream>
namespace threadpool_learn {

	Semaphore::Semaphore(int res_size)
		:res_size_(res_size)
	{}

	void Semaphore::post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		res_size_++;
		cond_.notify_all();
	}

	void Semaphore::wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// 等待信号量有资源，没有资源的话，会阻塞当前线程
		cond_.wait(lock, [&]()->bool {return res_size_ > 0; });
		res_size_--;
	}
}