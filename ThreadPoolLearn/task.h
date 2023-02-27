#pragma once
#include"any.h"
namespace threadpool_learn {

	struct InternalState;
	/*
	* 任务基类
	* 自定义任务实现run方法
	*/
	class Task
	{
	public:
		Task();
		~Task() = default;
		void exec();
		void setInternalState(std::shared_ptr<InternalState> p_internal_state);

		// 重写run方法处理任务
		virtual Any run() = 0;

	private:
		std::shared_ptr<InternalState> p_internal_state_;
	};
}