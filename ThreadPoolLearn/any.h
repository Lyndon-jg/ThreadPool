#pragma once
#include<memory>
namespace threadpool_learn {

	/*
	* 模拟实现c++ any类，接收任意类型参数
	*/
	class Any
	{
	public:
		Any() = default;
		~Any() = default;
		Any(const Any&) = delete;
		Any& operator=(const Any&) = delete;
		Any(Any&&) = default;
		Any& operator=(Any&&) = default;

		template<typename T>
		Any(T data) :p_base_(std::make_unique<Data<T>>(data)) {}

		template<typename T>
		T castAny() {
			Data<T>* p_data = dynamic_cast<Data<T>*>(p_base_.get());
			if (p_data == nullptr) {
				throw "type is not match, cast failed!!!";
			}
			return p_data->data();
		}

	private:
		class Base
		{
		public:
			Base() = default;
			virtual ~Base() = default;
		};

		template<typename T>
		class Data : public Base
		{
		public:
			Data(T data) : data_(data) {}
			T data() {
				return data_;
			}

		private:
			T data_;
		};

	private:
		std::unique_ptr<Base> p_base_;
	};
}