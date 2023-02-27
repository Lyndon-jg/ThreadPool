
#include<chrono>
#include<thread>
#include<iostream>

#include "threadpool_learn.h"
#include "threadpool.h"

using uLong = unsigned long long;

class MyTask : public threadpool_learn::Task
{
public:

	MyTask(int begin, int end)
		: begin_(begin)
		, end_(end)
	{}

	threadpool_learn::Any run()
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));

		uLong sum = 0;
		for (uLong i = begin_; i <= end_; i++) {
			sum += i;
		}

		return sum;
	}

private:
	int begin_;
	int end_;
};


int sum1(int a, int b)
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b;
}
int sum2(int a, int b, int c)
{
	std::this_thread::sleep_for(std::chrono::seconds(3));
	return a + b + c;
}

int main()
{
	{
		using namespace threadpool_learn;

		Result result1, result2, result3, result4, result5, result6, result7, result8, result9, result10;
		//{
			ThreadPool thread_pool;
			//thread_pool.setTaskQueMaxThreshHold(2);
			//thread_pool.setMode(PoolMode::MODE_CACHED);
			thread_pool.start();
			result1 = thread_pool.submitTask(std::make_shared<MyTask>(1, 100000));
			result2 = thread_pool.submitTask(std::make_shared<MyTask>(100001, 200000));
			result3 = thread_pool.submitTask(std::make_shared<MyTask>(200001, 300000));
			result4 = thread_pool.submitTask(std::make_shared<MyTask>(300001, 400000));
			result5 = thread_pool.submitTask(std::make_shared<MyTask>(400001, 500000));
			result6 = thread_pool.submitTask(std::make_shared<MyTask>(500001, 600000));
			result7 = thread_pool.submitTask(std::make_shared<MyTask>(600001, 700000));
			result8 = thread_pool.submitTask(std::make_shared<MyTask>(700001, 800000));
			result9 = thread_pool.submitTask(std::make_shared<MyTask>(800001, 900000));
			result10 = thread_pool.submitTask(std::make_shared<MyTask>(900001, 1000000));
		//}

		try
		{
			uLong r1 = result1.get().castAny<uLong>();
			uLong r2 = result2.get().castAny<uLong>();
			uLong r3 = result3.get().castAny<uLong>();
			uLong r4 = result4.get().castAny<uLong>();
			uLong r5 = result5.get().castAny<uLong>();
			uLong r6 = result6.get().castAny<uLong>();
			uLong r7 = result7.get().castAny<uLong>();
			uLong r8 = result8.get().castAny<uLong>();
			uLong r9 = result9.get().castAny<uLong>();
			uLong r10 = result10.get().castAny<uLong>();
			std::cout << "result : " << r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 + r9 + r10 << std::endl;

		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}

		uLong result = 0;
		for (int i = 0; i <= 1000000; i++) {
			result += i;
		}

		std::cout << "result : " << result << std::endl;
		getchar();
	}

//=========================================================================

	{
		std::future<int> result1, result2, result3;
		{
			using namespace threadpool;
			ThreadPool threadpool;
			//threadpool.setMode(PoolMode::MODE_CACHED);
			//threadpool.setTaskQueMaxThreshHold(1);
			threadpool.start(1);
			try
			{
				result1 = threadpool.submitTask(sum1, 1, 2);
				result2 = threadpool.submitTask(sum2, 1, 2, 3);
				result3 = threadpool.submitTask([](uLong begin, uLong end)->int {
					uLong sum = 0;
					for (uLong i = begin; i <= end; i++) {
						sum += i;
					}
					return sum;
				}, 1, 1000000);


				std::cout << "result1 = " << result1.get() << std::endl;
				std::cout << "result2 = " << result2.get() << std::endl;
				std::cout << "result3 = " << result3.get() << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
			}

		}

	}

	
}