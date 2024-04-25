
#include<iostream>
#include<chrono>
#include<thread>

#include"threadpool.h">


/*
��Щ��������ϣ���ܹ���ȡ�߳�ִ������÷���ֵ��
������
1 + ������ + 30000�ĺ�
thread1  1 + ... + 10000
thread2  10001 + ... + 20000
.....

main thread����ÿһ���̷߳����������䣬���ȴ��������귵�ؽ�����ϲ����յĽ������
*/

using uLong = unsigned long long;

class MyTask : public Task
{
public:
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}
    // ����һ����ô���run�����ķ���ֵ�����Ա�ʾ���������
    // Java Python   Object ���������������͵Ļ���
    // C++17 Any����
    Any run()  // run�������վ����̳߳ط�����߳���ȥ��ִ����!
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

    // �����������Result������
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

    //// ����task��ִ���꣬task����û�ˣ�������task�����Result����Ҳû�� ===========�� ����
    uLong sum1 = res1.get().cast_<uLong>();  // get������һ��Any���ͣ���ôת�ɾ���������أ�
    uLong sum2 = res2.get().cast_<uLong>();
    uLong sum3 = res3.get().cast_<uLong>();
    std::cout << sum1 + sum2 + sum3 << std::endl;
    }
    // Master - Slave�߳�ģ��
    // Master�߳������ֽ�����Ȼ�������Slave�̷߳�������
    // �ȴ�����Slave�߳�ִ�������񣬷��ؽ��
    // Master�̺߳ϲ����������������
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
//    // ��taskPtr��������������¸�ֵʱ��Task����ᱻ�Զ�����
//    return 0;
//}
