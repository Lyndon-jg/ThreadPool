#include "threadpool_learn.h"
#include<iostream>

namespace threadpool_learn {

	const int TASK_MAX_SIZE = INT32_MAX;
	const int THREAD_MAX_SIZE = 1024;
	const int THREAD_MAX_IDLE_TIME = 60; // ��λ����

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
		//�ȴ��̳߳��������߳̽���
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

		//�ȴ�������в���ʱ����������������
		//����ȴ�ʱ�䳬��1�룬�ж������ύʧ��
		if (!task_que_not_full_.wait_for(
			lock,
			std::chrono::seconds(1),
			[&]()->bool {return task_que_.size() < task_size_threshhold_; })
			) {
			std::cout << "thread id: " << std::this_thread::get_id() << ", �ύ����ʧ��" << std::endl;
			return Result(p_task, false);
		}

		//�������δ��������������������
		task_que_.push(p_task);
		//�������������֪ͨ�̴߳�������
		task_que_not_empty_.notify_all();

		if (pool_mode_ == PoolMode::MODE_CACHED
			&& task_que_.size() > idle_thread_size_
			&& current_thread_size_ < thread_size_threshhold_) {
			// �����µ��̶߳���
			auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
			auto thread_id = p_thread->getId();
			std::cout << "thread id: " << thread_id << ", MODE_CACHED ģʽ�� �������߳�" << std::endl;
			threads_.emplace(p_thread->getId(), std::move(p_thread));
			// �����߳�
			threads_[thread_id]->start();
			// �޸��̸߳�����صı���
			current_thread_size_++;
			idle_thread_size_++;
		}

		return Result(p_task);
	}

	void ThreadPool::start(int thread_size) {
		if (thread_size <= 0) {
			return;
		}

		std::cout << "thread id: " << std::this_thread::get_id() << ", �̳߳�����" << std::endl;

		is_pool_running_ = true;
		init_thread_size_ = thread_size;
		current_thread_size_ = thread_size;

		std::cout << "thread id: " << std::this_thread::get_id() << ", ��ʼ�̳߳��̸߳�����" << init_thread_size_ << std::endl;
		std::cout << "thread id: " << std::this_thread::get_id() << ", ��ǰ�̳߳��̸߳�����" << current_thread_size_ << std::endl;

		//�����߳�
		for (int i = 0; i < thread_size; i++) {
			auto p_thread = std::make_unique<TaskThread>(std::bind(&ThreadPool::handleTask, this, std::placeholders::_1));
			threads_.emplace(p_thread->getId(), std::move(p_thread));
		}

		//�����߳�
		for (int i = 0; i < thread_size; i++) {
			threads_[i]->start();
			idle_thread_size_++;
		}
	}

	void ThreadPool::handleTask(int thread_id) {

		auto last_time = std::chrono::high_resolution_clock().now();

		// �����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
		while (true)
		{
			std::shared_ptr<Task> p_task;
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
						// ��ʱ����
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
				p_task = task_que_.front();
				task_que_.pop();
				//ȡ��һ�������������в�����֪ͨ�����������
				task_que_not_full_.notify_all();
			}//���������ͷ���֮����ִ������


			std::cout << "thread id: " << thread_id << ", ��ʼִ������" << std::endl;
			if (p_task) {
				p_task->exec();
			}
			idle_thread_size_++;
			std::cout << "thread id: " << thread_id << ", ����ִ�����" << std::endl;
			// �����߳�ִ���������ʱ��
			last_time = std::chrono::high_resolution_clock().now();
		}
	}

}