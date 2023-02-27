# future、promise及packaged_task源码分析

文章中的//x 会与代码片段中的//x 相对应
## 1. future

    template<class _Ty>
	class future
		: public _State_manager<_Ty>	//1
	{	
	using _Mybase = _State_manager<_Ty>;
	future(const _Mybase& _State, _Nil) //3
		: _Mybase(_State, true)
		{	// construct from associated asynchronous state object
		}
			...
	public:
	_Ty get() //2
		{
		future _Local{_STD move(*this)};
		return (_STD move(_Local._Get_value()));
		}
			...
	};
//1  future本身继承自_State_manager  
//2  future提供的get方法也是调用_State_manager的_Get_value 方法

再看一下_State_manager类：

    template<class _Ty>
	class _State_manager
	{	
	public:
	_State_manager(const _State_manager& _Other, bool _Get_once = false) //4
		: _Assoc_state(nullptr)
		{	// construct from _Other
		_Copy_from(_Other);
		_Get_only_once = _Get_once;
		}
			... 
	_Ty& _Get_value() const   // 2
		{	// return the stored result or throw stored exception
		if (!valid())
			_Throw_future_error(
				make_error_code(future_errc::no_state));
		return (_Assoc_state->_Get_value(_Get_only_once));	// 2.1
		}

	void _Set_value(const _Ty& _Val, bool _Defer)  // 3
		{	// store a result
		if (!valid())
			_Throw_future_error(
				make_error_code(future_errc::no_state));
		_Assoc_state->_Set_value(_Val, _Defer); // 3.1
		}
		...
	void _Copy_from(const _State_manager& _Other) //5
		{	// copy stored associated asynchronous state object from _Other
		if (this != _STD addressof(_Other))
			{	// different, copy
			if (_Assoc_state)
				_Assoc_state->_Release();
			if (_Other._Assoc_state == nullptr)
				_Assoc_state = nullptr;
			else
				{	// do the copy
				_Other._Assoc_state->_Retain();
				_Assoc_state = _Other._Assoc_state;  //5.1
				_Get_only_once = _Other._Get_only_once;
				}
			}
		}
		...
	private:
		_Associated_state<_Ty> *_Assoc_state; // 1
	};
 //1	 _State_manager 拥有一个_Associated_state类型的指针  
//2 //3  _State_manager提供了_Set_value _Get_value 等方法,但实际调用的还是_Associated_state(//2.1 //3.1)的对应方法

再来看一下_Associated_state类：

    template<class _Ty>
	class _Associated_state
	{	// class for managing associated synchronous state
	public:
			...
	virtual _Ty& _Get_value(bool _Get_only_once) 		//1
		{	// return the stored result or throw stored exception
		unique_lock<mutex> _Lock(_Mtx);
		...
		while (!_Ready)
			_Cond.wait(_Lock); //1.1
		...
		return (_Result);
		}
			...
	void _Set_value(const _Ty& _Val, bool _At_thread_exit) 	//2
		{	// store a result
		unique_lock<mutex> _Lock(_Mtx);
		_Set_value_raw(_Val, &_Lock, _At_thread_exit);
		}

	void _Set_value_raw(const _Ty& _Val, unique_lock<mutex> *_Lock, bool _At_thread_exit) //3
		{	// store a result while inside a locked block
		.	...
		_Result = _Val; //3.1
		_Do_notify(_Lock, _At_thread_exit);
		}
			...
	public:
		_Ty _Result;
		mutex _Mtx;
		condition_variable _Cond;
		int _Ready;
		...
	virtual void _Do_notify(unique_lock<mutex> *_Lock, bool _At_thread_exit)		//4
		{	// notify waiting threads
		_Has_stored_result = true;
		if (_At_thread_exit)
			{	// notify at thread exit
			_Cond._Register(*_Lock, &_Ready);
			}
		else
			{	// notify immediately
			_Ready = true; //4.1
			_Cond.notify_all();  
			}
		}
		...
	};

_Associated_state 提供了_Get_value等各个方法的具体实现  
//1 在future调用get方法时最终调用_Associated_state 的_Get_value方法, 如果此时还没有接收到任务返回的结果，则阻塞在//1.1处，否则直接获取结果  
//2  执行_Set_value会调用//3 _Set_value_raw方法将结果放到 //3.1 _Result中，//4 4.1 将_Read置为true，并唤醒阻塞在//1.1处的线程去接收任务结果。


## 2. promise

    template<class _Ty>
	class promise
	{	// class that defines an asynchronous provider that holds a value
	public:
		...
	_NODISCARD future<_Ty> get_future() //2
		{	// return a future object that shares the associated
			// asynchronous state
		return (future<_Ty>(_MyPromise._Get_state_for_future(), _Nil())); //2.1
		}

	void set_value(const _Ty& _Val) //3
		{	// store result
		_MyPromise._Get_state_for_set()._Set_value(_Val, false);
		}
		...
	private:
	_Promise<_Ty> _MyPromise;	 //1
	};
//1 promise内部持有一个_Promise对象  
//2 get_future的feature方法会返回一个future，用_Promise的_Get_state_for_future函数返回值_State_manager进行的feature创建  
//3 set_value方法调用的也是_MyPromise 的 _State_manager 的_Set_value 方法

下面看一下_Promise类：

    template<class _Ty>
	class _Promise
	{	// class that implements core of promise
	public:
	_Promise(_Associated_state<_Ty> *_State_ptr)
		: _State(_State_ptr, false), //4
			_Future_retrieved(false)
		{	// construct from associated asynchronous state object
		}
			...
	_State_manager<_Ty>& _Get_state_for_set() //3
		{	
		...
		return (_State);
		}

	_State_manager<_Ty>& _Get_state_for_future() //2
		{	
		...
		return (_State);
		}
		...
	private:
	_State_manager<_Ty> _State;  //1
	};
//1 _Promise内部拥有一个_State_manager (分析见上)  
//2 返回_State_manager对象_State用来创建future时(promise //2.1)，先调用future的构造函数(future //3)，然后调用_State_manager的构造函数（_State_manager //4）,最终调用_State_manager的_Copy_from方法（_State_manager  //5 //5.1）将promise->_Promise->_State_manager 的_Associated_state赋值给新创建的future->_State_manager的_Associated_state，**由于_State_manager内部的_Associated_state一个指针所以future就和promise 指向了同一个_Associated_state** ，这样在其他线程中调用promise的set_value方法时最终会掉到_Associated_state的_Set_value方法，这样就完成了不同线程间的通信及任务结果传递！！！

## 3. packaged_task

    template<class _Ret,
	class... _ArgTypes>
	class packaged_task<_Ret(_ArgTypes...)>
	{	// class that defines an asynchronous provider that returns the
		// result of a call to a function object
	public:
	using _Ptype = typename _P_arg_type<_Ret>::type;
	using _MyPromiseType = _Promise<_Ptype>;
	using _MyStateManagerType = _State_manager<_Ptype>;
	using _MyStateType = _Packaged_state<_Ret(_ArgTypes...)>;
		...
	template<class _Fty2,
		class = enable_if_t<!is_same_v<remove_cv_t<remove_reference_t<_Fty2>>, packaged_task>>>
		explicit packaged_task(_Fty2&& _Fnarg) 
		: _MyPromise(new _MyStateType(_STD forward<_Fty2>(_Fnarg))) //0
		{	// construct from rvalue function object
		}
		...
	_NODISCARD future<_Ret> get_future() //2
		{	// return a future object that shares the associated
			// asynchronous state
		return (future<_Ret>(_MyPromise._Get_state_for_future(), _Nil()));
		}

	void operator()(_ArgTypes... _Args) //3
		{	// call the function object
		if (_MyPromise._Is_ready())
			_Throw_future_error(
				make_error_code(future_errc::promise_already_satisfied));
		_MyStateManagerType& _State = _MyPromise._Get_state_for_set(); 	//3.1
		_MyStateType *_Ptr = static_cast<_MyStateType *>(_State._Ptr());	//3.2
		_Ptr->_Call_immediate(_STD forward<_ArgTypes>(_Args)...);
		}
	private:
	_MyPromiseType _MyPromise; //1
	};

//0 创建packaged_task时先创建一个_Packaged_state(_Packaged_state //4 记录要执行的任务)，然后初始化_MyPromise（promise //4）  
//1  packaged_task内有一个_MyPromiseType （_Promise）对象。（和promise一样）  
//2 get_future方法同promise。（future对象和packaged_task指向同一个_Associated_state）  
//3 执行packaged_task时，//3.1 先获去_State_manager，然后//3.2 获取_Associated_state指针强转成_Packaged_state 类型指针，然后调用其_Call_immediate方法。

下面看一下_Packaged_state类：

    template<class _Ret,
	class... _ArgTypes>
	class _Packaged_state<_Ret(_ArgTypes...)>
		: public _Associated_state<_Ret> //1
	{	// class for managing associated asynchronous state for packaged_task
	public:
		...
	template<class _Fty2>
		_Packaged_state(const _Fty2& _Fnarg) //4
			: _Fn(_Fnarg)
		{	// construct from function object
		}
		...
	void _Call_immediate(_ArgTypes... _Args) //3
		{	// call function object
		_TRY_BEGIN
			// call function object and catch exceptions
			this->_Set_value(_Fn(_STD forward<_ArgTypes> (_Args)...), false); //3.1
		_CATCH_ALL
			// function object threw exception; record result
			this->_Set_exception(_STD current_exception(), false);
		_CATCH_END
		}
			...
	private:
	function<_Ret(_ArgTypes...)> _Fn;	 //2
	};

//1 _Packaged_state继承自_Associated_state  
//2 _Packaged_state持有实际的任务对象_Fn  
//3 执行_Call_immediate方法时,//3.1 this->_Set_value 调用的是_Associated_state 的_Set_value（即_Associated_state //2）

## 总结
future与promise或者packaged_task是通过_Associated_state 相关联的，它们都共同指向同一个_Associated_state，future的get方法最终会调用_Associated_state 的_Get_value 方法，promise或者packaged_task会调用到_Associated_state 的_Set_value 方法，如果用户先调用future的get方法那么会阻塞，直到promise或者packaged_task调用到_Set_value 方法后才能被唤醒获取任务执行结果，如果用户后调用future的get方法，则会直接获的任务执行结果。
