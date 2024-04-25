
#include<iostream>
#include<chrono>
#include<thread>

#include"threadpool.h">


/*
有些场景，是希望能够获取线程执行任务得返回值得
举例：
1 + 。。。 + 30000的和
thread1  1 + ... + 10000
thread2  10001 + ... + 20000
.....

main thread：给每一个线程分配计算的区间，并等待他们算完返回结果，合并最终的结果即可
*/

using uLong = unsigned long long;

class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}
    // 问题一：怎么设计run函数的返回值，可以表示任意的类型
    // Java Python   Object 是所有其它类类型的基类
    // C++17 Any类型
    Any run()  // run方法最终就在线程池分配的线程中去做执行了!
    {
        std::cout << "tid:" << std::this_thread::get_id()
            << "begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        uLong sum = 0;
        for (uLong i = begin_; i <= end_; i++)
            sum += i;
        std::cout << "tid:" << std::this_thread::get_id()
            << "end!" << std::endl;

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main()
{
    {
	ThreadPool pool;
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start(4);

    // 如何设计这里的Result机制呢
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

    //// 随着task被执行完，task对象没了，依赖于task对象的Result对象也没了 ===========》 所以
    uLong sum1 = res1.get().cast_<uLong>();  // get返回了一个Any类型，怎么转成具体的类型呢？
    uLong sum2 = res2.get().cast_<uLong>();
    uLong sum3 = res3.get().cast_<uLong>();
    std::cout << sum1 + sum2 + sum3 << std::endl;
    }
    // Master - Slave线程模型
    // Master线程用来分解任务，然后给各个Slave线程分配任务
    // 等待各个Slave线程执行完任务，返回结果
    // Master线程合并各个任务结果，输出
    //std::cout << (sum1 + sum2 + sum3) << std::endl;
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    //pool.submitTask(std::make_shared<MyTask>());
    getchar();
}





















//#include <memory>
//#include <iostream>
//
//class Task {
//public:
//    Task() { std::cout << "Task created\n"; }
//    ~Task() { std::cout << "Task destroyed\n"; }
//    void doWork() { std::cout << "Task is working\n"; }
//};
//
//int main() {
//    std::shared_ptr<Task> taskPtr(&Task());
//    taskPtr->doWork();
//    // 当taskPtr超出作用域或被重新赋值时，Task对象会被自动销毁
//    return 0;
//}
