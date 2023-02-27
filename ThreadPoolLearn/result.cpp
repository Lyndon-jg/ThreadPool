#include"result.h"
#include"Task.h"
namespace threadpool_learn {

	Result::Result(std::shared_ptr<Task> p_task, bool task_is_valid)
		: p_internal_state_(std::make_shared<InternalState>())
	{
		p_internal_state_->task_is_valid_ = task_is_valid;
		p_task->setInternalState(p_internal_state_);
	}

	Any Result::get() {
		return p_internal_state_->get();
	}
}