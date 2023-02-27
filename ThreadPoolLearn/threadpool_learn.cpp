#include "threadpool_learn.h"
#include<iostream>

namespace threadpool_learn {

	const int TASK_MAX_SIZE = INT32_MAX;
	const int THREAD_MAX_SIZE = 1024;
	const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒

	ThreadPool::ThreadPool()
		: thread_size_threshhold_(THREAD_MAX_SIZE)
		, init_thread_size_(0)
		, current_thread_size_(0)
		, task_size_threshhold_(TASK_MAX_SIZE)
		, pool_mode_(PoolMode::MODE_FIXED)
		, is_pool_running_(false)
		, idle_thread_size_(0)
	{}

	ThreadPool::~ThreadPool() {
		is_pool_running_ = false;

		task_que_not_empty_.notify_all();
		//等待线程池内所有线程结束
		std::unique_lock<std::mutex> lock(task_que_mtx_);
		cond_exit_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}


	void ThreadPool::setMode(PoolMode mode) {
		if (is_pool_running_) {
			return;
		}
		pool_mode_ = mode;
	}

	void ThreadPool::setTaskQueMaxThreshHold(int threshhold) {
		if (is_pool_running_) {
			return;
		}
		if (threshhold <= 0) {
			return;
		}
		task_size_threshhold_ = threshhold;
	}

	void ThreadPool::setThreadSizeThreshHold(int threshhold)
	{
		if (is_pool_running_) {
			return;
		}
		if (threshhold <= 0) {
			return;
		}
		if (pool_mode_ == PoolMode::MODE_CACHED)
		{
			thread_size_threshhold_ = threshhold;
		}
	}

	Result ThreadPool::submitTask(std::shared_ptr<Task> p_task) {

		std::unique_lock<std::mutex> lock(task_que_mtx_);

		//等待任务队列不满时将任务放入任务队列
		//如果等待时间超过1秒，判定任务提交失败
		if (!task_que_not_full_.wait_for(
			lock,
			std::chrono::seconds(1),
			[&]()->bool {return task_que_.size() < task_size_threshhold_; })
			) {
			std::cout << "thread id: " << std::this_thread::get_id() << ", 提交任务失败" << std::endl;
			return Result(p_task, false);
		}

		//任务队列未满，将任务放入任务队列
		task_que_.push(p_task);
		//任务队列有任务，通知线程处理任务
		task_que_not_empty_.notify_all();

		if (pool_mode_ == PoolMode::MODE_CACHED
			&& task_que_.size() > idle_thread_size_
			&& current_thread_size_ < thread_size_threshhold_) {
			// 创建新的线程对象
			auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
			auto thread_id = p_thread->getId();
			std::cout << "thread id: " << thread_id << ", MODE_CACHED 模式， 创建新线程" << std::endl;
			threads_.emplace(p_thread->getId(), std::move(p_thread));
			// 启动线程
			threads_[thread_id]->start();
			// 修改线程个数相关的变量
			current_thread_size_++;
			idle_thread_size_++;
		}

		return Result(p_task);
	}

	void ThreadPool::start(int thread_size) {
		if (thread_size <= 0) {
			return;
		}

		std::cout << "thread id: " << std::this_thread::get_id() << ", 线程池启动" << std::endl;

		is_pool_running_ = true;
		init_thread_size_ = thread_size;
		current_thread_size_ = thread_size;

		std::cout << "thread id: " << std::this_thread::get_id() << ", 初始线程池线程个数：" << init_thread_size_ << std::endl;
		std::cout << "thread id: " << std::this_thread::get_id() << ", 当前线程池线程个数：" << current_thread_size_ << std::endl;

		//创建线程
		for (int i = 0; i < thread_size; i++) {
			auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
			threads_.emplace(p_thread->getId(), std::move(p_thread));
		}

		//启动线程
		for (int i = 0; i < thread_size; i++) {
			threads_[i]->start();
			idle_thread_size_++;
		}
	}

	void ThreadPool::handleTask(int thread_id) {

		auto last_time = std::chrono::high_resolution_clock().now();

		// 所有任务必须执行完成，线程池才可以回收所有线程资源
		while (true)
		{
			std::shared_ptr<Task> p_task;
			{
				std::unique_lock<std::mutex> lock(task_que_mtx_);

				while (task_que_.size() == 0)
				{
					// 线程池结束，回收线程资源
					if (!is_pool_running_) {
						threads_.erase(thread_id);
						current_thread_size_--;
						idle_thread_size_--;
						cond_exit_.notify_all();

						std::cout << "thread id: " << thread_id << ",线程池结束，线程被回收" << std::endl;
						return;
					}

					// cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程结束回收掉（超过init_thread_size_数量的线程要进行回收）
					if (pool_mode_ == PoolMode::MODE_CACHED)
					{
						// 超时返回
						if (std::cv_status::timeout ==
							task_que_not_empty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& current_thread_size_ > init_thread_size_)
							{
								// 回收当前线程
								threads_.erase(thread_id);
								current_thread_size_--;
								idle_thread_size_--;

								std::cout << "thread id: " << thread_id << ", 任务空闲60s被回收" << std::endl;
								return;
							}
						}
					}
					else
					{
						//等待任务队列非空时，获取任务并处理
						task_que_not_empty_.wait(lock);
					}
				}

				idle_thread_size_--;
				p_task = task_que_.front();
				task_que_.pop();
				//取了一个任务后，任务队列不满，通知可以添加任务
				task_que_not_full_.notify_all();
			}//出作用域释放锁之后再执行任务


			std::cout << "thread id: " << thread_id << ", 开始执行任务" << std::endl;
			if (p_task) {
				p_task->exec();
			}
			idle_thread_size_++;
			std::cout << "thread id: " << thread_id << ", 任务执行完成" << std::endl;
			// 更新线程执行完任务的时间
			last_time = std::chrono::high_resolution_clock().now();
		}
	}

}