#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>

namespace threadpool {

	const int TASK_MAX_SIZE = INT32_MAX;
	const int THREAD_MAX_SIZE = 1024;
	const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒


	// 线程池工作模式
	enum class PoolMode
	{
		// 固定数量线程
		MODE_FIXED,
		// 线程数量可动态增长
		MODE_CACHED,
	};

	/*
	* 执行任务的线程
	*/
	class TaskThread
	{

	public:
		// 线程函数对象类型
		using TaskFunc = std::function<void(int)>;

		TaskThread(TaskFunc task)
			: task_(task)
			, thread_id_(generate_id_++)
		{}

		~TaskThread() = default;

		// 启动线程
		void start() {
			std::thread t(task_, thread_id_);
			t.detach();
		}

		// 获取线程id
		int getId() const {
			return thread_id_;
		}
	private:
		TaskFunc task_;
		static int generate_id_;
		// 线程id
		int thread_id_;
	};

	int TaskThread::generate_id_ = 0;


	class ThreadPool
	{
		// Task任务
		using Task = std::function<void()>;
	public:
		ThreadPool()
			: thread_size_threshhold_(THREAD_MAX_SIZE)
			, init_thread_size_(0)
			, current_thread_size_(0)
			, task_size_threshhold_(TASK_MAX_SIZE)
			, pool_mode_(PoolMode::MODE_FIXED)
			, is_pool_running_(false)
			, idle_thread_size_(0)
		{}

		~ThreadPool()
		{
			is_pool_running_ = false;

			task_que_not_empty_.notify_all();
			//等待线程池内所有线程结束
			std::unique_lock<std::mutex> lock(task_que_mtx_);
			cond_exit_.wait(lock, [&]()->bool {return threads_.size() == 0; });
		}

		// 设置工作模式
		void setMode(PoolMode mode)
		{
			if (is_pool_running_) {
				return;
			}
			pool_mode_ = mode;
		}

		// 设置task任务队列上限阈值
		void setTaskQueMaxThreshHold(int threshhold)
		{
			if (is_pool_running_) {
				return;
			}
			if (threshhold <= 0) {
				return;
			}
			task_size_threshhold_ = threshhold;
		}

		// 设置线程池cached模式下线程阈值
		void setThreadSizeThreshHold(int threshhold)
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

		// 提交任务
		template<typename Func, typename... Args>
		auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
		{
			// 创建任务及feature
			using ReturnType = decltype(func(args...));
			auto task = std::make_shared<std::packaged_task<ReturnType()>>(
				std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
			std::future<ReturnType> result = task->get_future();

			// 获取锁
			std::unique_lock<std::mutex> lock(task_que_mtx_);

			// 等待任务队列不满时将任务放入任务队列
			// 如果等待时间超过1秒，判定任务提交失败
			if (!task_que_not_full_.wait_for(
				lock,
				std::chrono::seconds(1),
				[&]()->bool {return task_que_.size() < task_size_threshhold_; })
				) {

				//auto task = std::make_shared<std::packaged_task<ReturnType()>>(
					//[]()->ReturnType { return {}; });
				//(*task)();

				std::cout << "thread id: " << std::this_thread::get_id() << ", 提交任务失败" << std::endl;
				//return task->get_future();

				throw std::runtime_error("submit task failed.");
			}

			// 任务队列未满，将任务放入任务队列
			task_que_.push([task]() {(*task)(); });
			//任务队列有任务，通知线程处理任务
			task_que_not_empty_.notify_all();

			if (pool_mode_ == PoolMode::MODE_CACHED
				&& task_que_.size() > idle_thread_size_
				&& current_thread_size_ < thread_size_threshhold_) {
				// 创建新的线程对象
				auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
				auto thread_id = p_thread->getId();
				std::cout << "thread id: " << thread_id << ", MODE_CACHED 模式， 创建新线程" << std::endl;
				threads_.emplace(thread_id, std::move(p_thread));
				// 启动线程
				threads_[thread_id]->start();
				// 修改线程个数相关的变量
				current_thread_size_++;
				idle_thread_size_++;
			}
			return result;
		}

		// 开启线程池
		void start(int thread_size = std::thread::hardware_concurrency())
		{
			if (thread_size <= 0) {
				return;
			}
			std::cout << "thread id: " << std::this_thread::get_id() << ", 线程池启动" << std::endl;

			is_pool_running_ = true;
			init_thread_size_ = thread_size;
			current_thread_size_ = thread_size;

			std::cout << "thread id: " << std::this_thread::get_id() << ", 初始线程池线程个数：" << init_thread_size_ << std::endl;
			std::cout << "thread id: " << std::this_thread::get_id() << ", 当前线程池线程个数：" << current_thread_size_ << std::endl;

			// 创建线程
			for (int i = 0; i < thread_size; i++)
			{
				// 创建thread线程对象的时候，把线程函数给到thread线程对象
				auto ptr = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
				threads_.emplace(ptr->getId(), std::move(ptr));
			}

			// 启动线程
			for (int i = 0; i < thread_size; i++)
			{
				threads_[i]->start();
				idle_thread_size_++;
			}
		}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

	private:
		// 定义线程函数
		void handleTask(int thread_id)
		{
			auto last_time = std::chrono::high_resolution_clock().now();

			while (true)
			{
				Task task;
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
							// 条件变量，超时返回了
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
					task = task_que_.front();
					task_que_.pop();
					//取了一个任务后，任务队列不满，通知可以添加任务
					task_que_not_full_.notify_all();
				} //出作用域释放锁之后再执行任务;

				std::cout << "thread id: " << thread_id << ", 开始执行任务" << std::endl;
				if (task != nullptr)
				{
					task();
				}
				idle_thread_size_++;
				std::cout << "thread id: " << thread_id << ", 任务执行完成" << std::endl;
				// 更新线程执行完任务的时间
				last_time = std::chrono::high_resolution_clock().now();
			}
		}

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
		std::queue<Task> task_que_;
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