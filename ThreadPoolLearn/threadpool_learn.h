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

	// �̳߳ع���ģʽ
	enum class PoolMode
	{
		// �̶������߳�
		MODE_FIXED,
		// �߳������ɶ�̬����
		MODE_CACHED,
	};

	class ThreadPool
	{
	public:
		ThreadPool();
		~ThreadPool();

		// ���ù���ģʽ
		void setMode(PoolMode mode);

		// ����task�������������ֵ
		void setTaskQueMaxThreshHold(int threshhold);

		// �����̳߳�cachedģʽ���߳���ֵ
		void setThreadSizeThreshHold(int threshhold);

		// �����̳߳�
		void start(int thread_size = std::thread::hardware_concurrency());

		// �û��ύ���񣬽���������������
		Result submitTask(std::shared_ptr<Task> p_task);

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

	private:
		//�����������������߳�ִ�и÷�������������л�ȡ����ִ��
		void handleTask(int thread_id);

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
		std::queue<std::shared_ptr<Task>> task_que_;
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