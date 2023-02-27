#include"task.h"
#include "result.h"
namespace threadpool_learn {

	Task::Task()
		: p_internal_state_(nullptr)
	{}

	void Task::exec() {
		if (p_internal_state_) {
			p_internal_state_->set(run());
		}
	}

	void Task::setInternalState(std::shared_ptr<InternalState> p_internal_state)
	{
		p_internal_state_ = p_internal_state;
	}

}