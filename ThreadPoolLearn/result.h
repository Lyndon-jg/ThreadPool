#pragma once
#include<memory>
#include<atomic>

#include"any.h"
#include "semaphore.h"
namespace threadpool_learn {

	class Task;

	struct InternalState {
		//��any�洢����ķ���ֵ
		Any any_;
		//�߳�ͨ���ź���
		Semaphore sem_;
		//����ֵ�Ƿ���Ч
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
	* ��¼����ִ�з��ؽ��,getʱ�������û��ִ�����������������������ִ������򷵻ؽ��
	*/
	class Result
	{
	public:
		Result() = default;
		Result(std::shared_ptr<Task> p_task, bool task_is_valid = true);
		~Result() = default;

		//��ȡ������
		Any get();

	private:
		std::shared_ptr<InternalState> p_internal_state_;
	};
}