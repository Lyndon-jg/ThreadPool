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
	const int THREAD_MAX_IDLE_TIME = 60; // ��λ����


	// �̳߳ع���ģʽ
	enum class PoolMode
	{
		// �̶������߳�
		MODE_FIXED,
		// �߳������ɶ�̬����
		MODE_CACHED,
	};

	/*
	* ִ��������߳�
	*/
	class TaskThread
	{

	public:
		// �̺߳�����������
		using TaskFunc = std::function<void(int)>;

		TaskThread(TaskFunc task)
			: task_(task)
			, thread_id_(generate_id_++)
		{}

		~TaskThread() = default;

		// �����߳�
		void start() {
			std::thread t(task_, thread_id_);
			t.detach();
		}

		// ��ȡ�߳�id
		int getId() const {
			return thread_id_;
		}
	private:
		TaskFunc task_;
		static int generate_id_;
		// �߳�id
		int thread_id_;
	};

	int TaskThread::generate_id_ = 0;


	class ThreadPool
	{
		// Task����
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
			//�ȴ��̳߳��������߳̽���
			std::unique_lock<std::mutex> lock(task_que_mtx_);
			cond_exit_.wait(lock, [&]()->bool {return threads_.size() == 0; });
		}

		// ���ù���ģʽ
		void setMode(PoolMode mode)
		{
			if (is_pool_running_) {
				return;
			}
			pool_mode_ = mode;
		}

		// ����task�������������ֵ
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

		// �����̳߳�cachedģʽ���߳���ֵ
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

		// �ύ����
		template<typename Func, typename... Args>
		auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
		{
			// ��������feature
			using ReturnType = decltype(func(args...));
			auto task = std::make_shared<std::packaged_task<ReturnType()>>(
				std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
			std::future<ReturnType> result = task->get_future();

			// ��ȡ��
			std::unique_lock<std::mutex> lock(task_que_mtx_);

			// �ȴ�������в���ʱ����������������
			// ����ȴ�ʱ�䳬��1�룬�ж������ύʧ��
			if (!task_que_not_full_.wait_for(
				lock,
				std::chrono::seconds(1),
				[&]()->bool {return task_que_.size() < task_size_threshhold_; })
				) {

				//auto task = std::make_shared<std::packaged_task<ReturnType()>>(
					//[]()->ReturnType { return {}; });
				//(*task)();

				std::cout << "thread id: " << std::this_thread::get_id() << ", �ύ����ʧ��" << std::endl;
				//return task->get_future();

				throw std::runtime_error("submit task failed.");
			}

			// �������δ��������������������
			task_que_.push([task]() {(*task)(); });
			//�������������֪ͨ�̴߳�������
			task_que_not_empty_.notify_all();

			if (pool_mode_ == PoolMode::MODE_CACHED
				&& task_que_.size() > idle_thread_size_
				&& current_thread_size_ < thread_size_threshhold_) {
				// �����µ��̶߳���
				auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
				auto thread_id = p_thread->getId();
				std::cout << "thread id: " << thread_id << ", MODE_CACHED ģʽ�� �������߳�" << std::endl;
				threads_.emplace(thread_id, std::move(p_thread));
				// �����߳�
				threads_[thread_id]->start();
				// �޸��̸߳�����صı���
				current_thread_size_++;
				idle_thread_size_++;
			}
			return result;
		}

		// �����̳߳�
		void start(int thread_size = std::thread::hardware_concurrency())
		{
			if (thread_size <= 0) {
				return;
			}
			std::cout << "thread id: " << std::this_thread::get_id() << ", �̳߳�����" << std::endl;

			is_pool_running_ = true;
			init_thread_size_ = thread_size;
			current_thread_size_ = thread_size;

			std::cout << "thread id: " << std::this_thread::get_id() << ", ��ʼ�̳߳��̸߳�����" << init_thread_size_ << std::endl;
			std::cout << "thread id: " << std::this_thread::get_id() << ", ��ǰ�̳߳��̸߳�����" << current_thread_size_ << std::endl;

			// �����߳�
			for (int i = 0; i < thread_size; i++)
			{
				// ����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
				auto ptr = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
				threads_.emplace(ptr->getId(), std::move(ptr));
			}

			// �����߳�
			for (int i = 0; i < thread_size; i++)
			{
				threads_[i]->start();
				idle_thread_size_++;
			}
		}

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

	private:
		// �����̺߳���
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
						// �̳߳ؽ����������߳���Դ
						if (!is_pool_running_) {
							threads_.erase(thread_id);
							current_thread_size_--;
							idle_thread_size_--;
							cond_exit_.notify_all();

							std::cout << "thread id: " << thread_id << ",�̳߳ؽ������̱߳�����" << std::endl;
							return;
						}
						// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳̽������յ�������init_thread_size_�������߳�Ҫ���л��գ�
						if (pool_mode_ == PoolMode::MODE_CACHED)
						{
							// ������������ʱ������
							if (std::cv_status::timeout ==
								task_que_not_empty_.wait_for(lock, std::chrono::seconds(1)))
							{
								auto now = std::chrono::high_resolution_clock().now();
								auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
								if (dur.count() >= THREAD_MAX_IDLE_TIME
									&& current_thread_size_ > init_thread_size_)
								{
									// ���յ�ǰ�߳�
									threads_.erase(thread_id);
									current_thread_size_--;
									idle_thread_size_--;

									std::cout << "thread id: " << thread_id << ", �������60s������" << std::endl;
									return;
								}
							}
						}
						else
						{
							//�ȴ�������зǿ�ʱ����ȡ���񲢴���
							task_que_not_empty_.wait(lock);
						}
					}

					idle_thread_size_--;
					task = task_que_.front();
					task_que_.pop();
					//ȡ��һ�������������в�����֪ͨ�����������
					task_que_not_full_.notify_all();
				} //���������ͷ���֮����ִ������;

				std::cout << "thread id: " << thread_id << ", ��ʼִ������" << std::endl;
				if (task != nullptr)
				{
					task();
				}
				idle_thread_size_++;
				std::cout << "thread id: " << thread_id << ", ����ִ�����" << std::endl;
				// �����߳�ִ���������ʱ��
				last_time = std::chrono::high_resolution_clock().now();
			}
		}

	private:
		// �߳�map
		std::unordered_map<int, std::unique_ptr<TaskThread>> threads_;
		// �߳�������ֵ
		int thread_size_threshhold_;
		//��ʼ�߳�����
		int init_thread_size_;
		//�����߳�����
		std::atomic_uint idle_thread_size_;
		// ��ǰ�̳߳������̵߳�������
		std::atomic_uint current_thread_size_;

		//�������
		std::queue<Task> task_que_;
		//������������
		int task_size_threshhold_;
		std::mutex task_que_mtx_;
		// ������в���
		std::condition_variable task_que_not_full_;
		// ������в���
		std::condition_variable task_que_not_empty_;

		//�߳�����״̬
		std::atomic_bool is_pool_running_;
		//�̹߳���ģʽ
		PoolMode pool_mode_;
		//�̳߳�����ʱ�ȴ��߳���Դȫ������
		std::condition_variable cond_exit_;
	};

}