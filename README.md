# ThreadPool
c++ 14实现的线程池
实现线程池逻辑，支持固定数量线程和线程数量可变两种模式及其他配置

1. **ThreadPoolLearn**
	分析了[future、promise、packaged_task源码](https://github.com/Lyndon-jg/ThreadPool/blob/main/ThreadPoolLearn/future%E3%80%81promise%E5%8F%8Apackaged_task%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90.md)，自己实现Any类接收任意执行结果类型，实现Semaphore类对获取结果线程进行阻塞，实现Result类来模拟future的功能，实现获取任务返回结果时，如果任务未执行完则阻塞，等待任务执行完成，若任务以执行完则直接获取执行结果。

2. **ThreadPool**
	通过可变参模板实现线程池submitTask接口，支持任意任务函数和任意参数的传递，使用future接收submitTask提交任务的返回值

