#pragma once

#include<vector>
#include<memory>
#include<queue>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<unordered_map>


#include "taskthread.h"
#include "Task.h"
#include "result.h"

namespace threadpool_learn {

	// 线程池工作模式
	enum class PoolMode
	{
		// 固定数量线程
		MODE_FIXED,
		// 线程数量可动态增长
		MODE_CACHED,
	};

	class ThreadPool
	{
	public:
		ThreadPool();
		~ThreadPool();

		// 设置工作模式
		void setMode(PoolMode mode);

		// 设置task任务队列上限阈值
		void setTaskQueMaxThreshHold(int threshhold);

		// 设置线程池cached模式下线程阈值
		void setThreadSizeThreshHold(int threshhold);

		// 开启线程池
		void start(int thread_size = std::thread::hardware_concurrency());

		// 用户提交任务，将任务放入任务队列
		Result submitTask(std::shared_ptr<Task> p_task);

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

	private:
		//处理任务函数，各个线程执行该方法，从任务队列获取任务并执行
		void handleTask(int thread_id);

	private:

		// 线程map
		std::unordered_map<int, std::unique_ptr<TaskThread>> threads_;
		// 线程数量阈值
		int thread_size_threshhold_;
		//初始线程数量
		int init_thread_size_;
		//空闲线程数量
		std::atomic_uint idle_thread_size_;
		// 当前线程池里面线程的总数量
		std::atomic_uint current_thread_size_;

		//任务队列
		std::queue<std::shared_ptr<Task>> task_que_;
		//任务数量上线
		int task_size_threshhold_;
		std::mutex task_que_mtx_;
		// 任务队列不满
		std::condition_variable task_que_not_full_;
		// 任务队列不空
		std::condition_variable task_que_not_empty_;

		//线程运行状态
		std::atomic_bool is_pool_running_;
		//线程工作模式
		PoolMode pool_mode_;
		//线程池析构时等待线程资源全部回收
		std::condition_variable cond_exit_;

	};

}