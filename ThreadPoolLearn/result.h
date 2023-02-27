#pragma once
#include<memory>
#include<atomic>

#include"any.h"
#include "semaphore.h"
namespace threadpool_learn {

	class Task;

	struct InternalState {
		//用any存储任务的返回值
		Any any_;
		//线程通信信号量
		Semaphore sem_;
		//返回值是否有效
		std::atomic_bool task_is_valid_ = true;

		void set(Any any) {
			any_ = std::move(any);
			sem_.post();
		}

		Any get() {
			if (task_is_valid_) {
				sem_.wait();
				return std::move(any_);
			}
			else {
				//return {};
				throw std::runtime_error("task is invalid.");
			}
		}
	};

	/*
	* 记录任务执行返回结果,get时如果任务没有执行完成则进行阻塞，如果任务执行完成则返回结果
	*/
	class Result
	{
	public:
		Result() = default;
		Result(std::shared_ptr<Task> p_task, bool task_is_valid = true);
		~Result() = default;

		//获取任务结果
		Any get();

	private:
		std::shared_ptr<InternalState> p_internal_state_;
	};
}