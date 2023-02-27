#include"taskthread.h"
#include<thread>
namespace threadpool_learn {

	int TaskThread::generate_id_ = 0;

	TaskThread::TaskThread(TaskFunc task)
		: task_(task)
		, thread_id_(generate_id_++)
	{}

	void TaskThread::start() {
		std::thread t(task_, thread_id_);
		t.detach();
	}

	int TaskThread::getId() const
	{
		return thread_id_;
	}

}