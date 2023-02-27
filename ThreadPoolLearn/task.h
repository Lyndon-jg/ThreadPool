#pragma once
#include"any.h"
namespace threadpool_learn {

	struct InternalState;
	/*
	* �������
	* �Զ�������ʵ��run����
	*/
	class Task
	{
	public:
		Task();
		~Task() = default;
		void exec();
		void setInternalState(std::shared_ptr<InternalState> p_internal_state);

		// ��дrun������������
		virtual Any run() = 0;

	private:
		std::shared_ptr<InternalState> p_internal_state_;
	};
}