//#pragma once               // ����һ�� ��ֹͷ�ļ����ظ�����
#ifndef THREADPOOL_H         // �������� ��ֹͷ�ļ����ظ�����
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>

// C++17 ��׼������� std::any ���͡�����ҪĿ��������һ������������ʱ���в������κ����͵�ֵ��ͬʱҲ�ṩ�����Ͱ�ȫ�ķ�ʽ���������޸Ĵ洢��ֵ
//Any���ͣ����Խ����������ݵ�����  (����C++17��Any����)
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
		
	// ������캯��������Any���ͽ�����������������
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data))   // Any(T data) :base_(new Derive<T>(data))
	{

	}

	// ��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		// ������ô��base_�ҵ�����ָ���Derive���󣬴�������ȡ��data��Ա����
		// ����ָ�� =�� ������ָ��   RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}

private:
	// ��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// ����������
	template<class T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data)
		{

		}
		T data_;
	};

private:
	// ����һ������ָ��
	std::unique_ptr<Base> base_;
};

// Semaphore  ʵ��һ���ź�����  ������C++20������ź�����
class Semaphore
{
public:
	Semaphore(int limit=0) :resLimit_(limit)
	{

	}
	~Semaphore() = default;
	// ��ȡһ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;

		cv_.notify_all();
	}
	// ����һ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		// �ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
		cv_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
private:
	int resLimit_;   // ��Դ����
	std::mutex mtx_;
	std::condition_variable cv_;
};


// Task���͵�ǰ������
class Task;

// ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid=true);
	~Result() = default;

	// ����һ��setVal��������ȡ����ִ����ķ���ֵ��
	void setVal(Any any);

	// �������get�������û��������������ȡtask�ķ���ֵ
	Any get();
private:
	Any any_; // �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<Task> task_; //ָ���Ӧ��ȡ����ֵ��������� 
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч
};


// ����������
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);

	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;

private:
	Result* result_; // Result�������������  >  Task��          ���ﲻ��������ָ�루����ɽ�������====���ڴ�й©��
};


// �̳߳�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,         // �̶��������߳�
	MODE_CACHED,        // �߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:
	//�̺߳�����������
	using ThreadFunc = std::function<void(int)>;
	//���캯��
	Thread(ThreadFunc func);
	// ��������
	~Thread();
	// �����߳�
	void start();
	// ��ȡ�߳�id
	int getId()const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_;  // �����߳�id
};


/*
example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task
{
	public:
		void run() { // �̴߳���... }
};

pool.submitTask(std::make_shared<MyTask>());
*/

// �̳߳�����
class ThreadPool
{
public:
	// �̳߳ع���
	ThreadPool();
	// �̳߳�����
	~ThreadPool();

	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode);
	// ����task������е�������ֵ
	void setTaskQueMaxThreshHold(size_t threshold);
	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHoid(int threshold);
	// ���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(size_t initThreadSize= std::thread::hardware_concurrency());    // std::thread::hardware_concurrency() ��ȡʵ��֧�ֵĲ����߳�����(CPU ��������)

	ThreadPool(const ThreadPool&) = delete;               //�ž��̳߳ض���Ŀ�������͸�ֵ���캯����Ҫʹ���̳߳�ֱ�ӹ��켴��
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//�����̺߳���
	void threadFunc(int threadid);

	// ���pool������״̬
	bool checkRunningState() const;
private:
	//std::vector<Thread*> threads_;    //�߳��б�
	//std::vector<std::unique_ptr<Thread>> threads_;  //�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;   //�߳��б�

	size_t initThreadSize_;             // ��ʼ���߳�����
	std::atomic_int curThreadSize_;     // ��¼��ǰ�̳߳������̵߳�������
	int threadSizeThresHold_;           //�߳��������� 
	std::atomic_int idleThreadSize_;    //��¼�����̵߳�����

	std::queue<std::shared_ptr<Task>> taskQue_;         // �������           ���ܴ�Task����ָ��std::queue<Task*> ===������û�������������ʱ�����ǽ������������⣬ʹ������ָ��ǿ���û����ܴ�����ֵ(��ʱ����)
	std::atomic_uint taskSize_;       //���������
	size_t taskQueMaxThreshHold_;     // �����������������ֵ

	std::mutex taskQueMtx_;                    // ��֤��������̰߳�ȫ
	std::condition_variable notFull_;          // ��ʾ������в���
	std::condition_variable notEmpty_;         // ��ʾ������в���
	std::condition_variable exitCond_;     // �ȵ��߳���Դȫ������

	PoolMode poolMode_;                  // ��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_;       //��ʾ��ǰ�̳߳ص�����״̬
};




#endif // !THREADPOOL_H


