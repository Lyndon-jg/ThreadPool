#pragma once
#include<functional>

namespace threadpool_learn {

	/*
	* ִ��������߳�
	*/
	class TaskThread
	{
	public:
		// �̺߳�����������
		using TaskFunc = std::function<void(int)>;

		TaskThread(TaskFunc task);

		~TaskThread() = default;

		// �����߳�
		void start();

		// ��ȡ�߳�id
		int getId() const;
	private:
		TaskFunc task_;
		static int generate_id_;
		// �߳�id
		int thread_id_;
	};

}