#pragma once
#include<mutex>
#include<condition_variable>
#include<memory>

namespace threadpool_learn {

	/*
	�ź�����
	����ִ��������̺߳ͻ�ȡ����ִ�н�����߳�֮���ͨ��
	*/
	class Semaphore
	{
	public:
		Semaphore(int res_size = 0);
		~Semaphore() = default;

		// ��ȡһ���ź�����Դ
		void wait();

		// ����һ���ź�����Դ
		void post();
	private:
		int res_size_;
		std::mutex mtx_;
		std::condition_variable cond_;
	};

}