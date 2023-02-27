#pragma once
#include<functional>

namespace threadpool_learn {

	/*
	* 执行任务的线程
	*/
	class TaskThread
	{
	public:
		// 线程函数对象类型
		using TaskFunc = std::function<void(int)>;

		TaskThread(TaskFunc task);

		~TaskThread() = default;

		// 启动线程
		void start();

		// 获取线程id
		int getId() const;
	private:
		TaskFunc task_;
		static int generate_id_;
		// 线程id
		int thread_id_;
	};

}