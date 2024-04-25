// �̳߳���Ŀ-���հ�.cpp : ���ļ����� "main" ����������ִ�н��ڴ˴���ʼ��������
//

#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
using namespace std;

#include "threadpool.h"


/*
��������̳߳��ύ������ӷ���
1. pool.submitTask(sum1, 10, 20);
   pool.submitTask(sum2, 1 ,2, 3);
   submitTask:�ɱ��ģ����

2. �����Լ�����һ��Result�Լ���ص����ͣ�����ͦ��
    C++11 �߳̿�   thread   packaged_task(function��������)  async
   ʹ��future������Result��ʡ�̳߳ش���
*/

int sum1(int a, int b)
{
    this_thread::sleep_for(chrono::seconds(2));
    // �ȽϺ�ʱ
    return a + b;
}
int sum2(int a, int b, int c)
{
    this_thread::sleep_for(chrono::seconds(2));
    return a + b + c;
}
// io�߳� 
void io_thread(int listenfd)
{

}
// worker�߳�
void worker_thread(int clientfd)
{

}
int main()
{
    ThreadPool pool;
    // pool.setMode(PoolMode::MODE_CACHED);
    pool.start(2);

    future<int> r1 = pool.submitTask(sum1, 1, 2);
    future<int> r2 = pool.submitTask(sum2, 1, 2, 3);
    future<int> r3 = pool.submitTask([](int b, int e)->int {
        int sum = 0;
        for (int i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 1, 100);
    future<int> r4 = pool.submitTask([](int b, int e)->int {
        int sum = 0;
        for (int i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 1, 100);
    future<int> r5 = pool.submitTask([](int b, int e)->int {
        int sum = 0;
        for (int i = b; i <= e; i++)
            sum += i;
        return sum;
        }, 1, 100);
    //future<int> r4 = pool.submitTask(sum1, 1, 2);

    cout << r1.get() << endl;    //�����ȴ��߳�ִ����ɣ�����ȡ�������Ľ��
    cout << r2.get() << endl;
    cout << r3.get() << endl;
    cout << r4.get() << endl;
    cout << r5.get() << endl;

    //packaged_task<int(int, int)> task(sum1);
    //// future <=> Result
    //future<int> res = task.get_future();
    //// task(10, 20);
    //thread t(std::move(task), 10, 20);
    //t.detach();

    //cout << res.get() << endl;        // ����������ȴ�����ִ�����

    /*thread t1(sum1, 10, 20);
    thread t2(sum2, 1, 2, 3);

    t1.join();
    t2.join();*/
}