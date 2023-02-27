#pragma once
#include<mutex>
#include<condition_variable>
#include<memory>

namespace threadpool_learn {

	/*
	信号量类
	用于执行任务的线程和获取任务执行结果的线程之间的通信
	*/
	class Semaphore
	{
	public:
		Semaphore(int res_size = 0);
		~Semaphore() = default;

		// 获取一个信号量资源
		void wait();

		// 增加一个信号量资源
		void post();
	private:
		int res_size_;
		std::mutex mtx_;
		std::condition_variable cond_;
	};

}