#include"threadpool.h"
#include<functional>
#include<thread>
#include<iostream>
#include<chrono>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 100;     
const int THREAD_MAX_IDLE_TIME = 6;    // ��λ:��

// �̳߳ع���
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	,taskSize_(0)
	,idleThreadSize_(0)
	,curThreadSize_(0)
	,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	,threadSizeThresHold_(THREAD_MAX_THRESHOLD)
	,poolMode_(PoolMode::MODE_FIXED)
	,isPoolRunning_(false)
{

}

// �̳߳�����
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;

	// �ȴ��̳߳��������е��̷߳���  ������״̬������ & ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}


// �����̳߳ع���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}

// ����task������е�������ֵ
void ThreadPool::setTaskQueMaxThreshHold(size_t threshold)
{
	if (checkRunningState())     // ���������к���������
	{
		return;
	}
	taskQueMaxThreshHold_ = threshold;
}

// �����̳߳�cachedģʽ���߳���ֵ
void ThreadPool::setThreadSizeThreshHoid(int threshold)
{
	if (checkRunningState())     // ���������к���������
	{
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThresHold_ = threshold;
	}
}

// ���̳߳��ύ����    �û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// �ٻ�ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// ���̵߳�ͨ��   �ȴ���������п���
	//while (taskQue_.size() == taskQueMaxThreshHold_)
	//{
	//	notFull_.wait(lock);   // 1���ͷŵ�ǰ��  2�����̽���ȴ�״̬
	//}
	
	//���д�СС�������ֵʱ������true�������̼߳���ִ�С����ν�ʷ���false�����̻߳��ٴν���ȴ�״̬��
	// ������ذ汾��wait()�ڲ�ʵ������һ��ѭ������������ٻ��ѷ��������¼��������ȷ��ֻ����������������ʱ�̲߳Ż����ִ��
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	// wait()  ���������һֱ�ȴ���ֱ����һ���̵߳������������� notify_one() �� notify_all() ����Ϊֹ���ڵȴ��ڼ䣬��ǰ�̻߳��ͷ����������������������ϡ�����������ʱ��wait() �������أ������»�ȡ���� 
	// wait_for()  ������ȴ�һ��ָ����ʱ�䣬�����ָ����ʱ������������û�б����㣬�����᷵�ء����ֵȴ������޵ģ�����һֱ�����̡߳���������ڳ�ʱǰ�����㣬��������ǰ����
	//wait_until()==������һ������ʱ�����Ϊ������������һ�����ʱ��Ρ�����ȴ�ֱ��ָ����ʱ��㣬����ֱ�����������㡣
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))         // wait()    wait_for()   wait_until()==������һ������ʱ�����Ϊ������������һ�����ʱ��Ρ�����ȴ�ֱ��ָ����ʱ��㣬����ֱ�����������㡣
	{
		// ��ʾnotFull_�ȴ�1s�ӣ�������Ȼ������
		std::cerr << "task queue is full,submit task fail." << std::endl;
		// return task->getResult();                         // Task Result �߳�ִ����task,task������������ˣ����Բ��ܲ������ַ�ʽ
		return Result(sp,false);         //
	}


	// ������п��࣬ ������������������
	taskQue_.emplace(sp);
	taskSize_++;

	// ����Ϊ�����µ�����������п϶������ˣ���notEmpty_��֪ͨ,�Ͽ�����߳�ִ������
	notEmpty_.notify_all();

	// ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳���
	// cachedģʽ  ������ƽϽ���  ������С���������        ʱ�䳤������̫ʱ��ʹ��cachedģʽ����Ϊ����������ʱ��ᴴ��������̣߳���ʱ������᳤ʱ��ռ���̣߳�������ϵͳ����
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThresHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// �������߳�
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));       // std::make_unique��C++14�������һ������ģ��
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// �����߳�
		threads_[threadId]->start();
		// �޸��̸߳�����صı���
		curThreadSize_++;
		idleThreadSize_++;
		/*threads_.emplace_back(std::move(ptr));*/
	}

	// ���������Result����
	return Result(sp);

}

// �����̳߳�
void ThreadPool::start(size_t initThreadSize)
{
	// �����̳߳ص�����״̬
	isPoolRunning_ = true;
	// ��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// �����̶߳���(��ÿ�����߳�)
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// ����thread�̶߳����ʱ�򣬰��̺߳�������thread����        �̶߳���ִ��ʲô�������̳߳ض������
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));           // ʹ��Thread��ָ��������    

		// ʹ��unique_ptr����ָ�����Thread����   ===��   ֱ��ʹ��new���ܵ��´��������ǵ���delete��ʹ��std::unique_ptr�����Զ������ڴ棬��ֹ�������
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));       // std::make_unique��C++14�������һ������ģ��
		//threads_.emplace_back(std::move(ptr));  // vector
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));   //map
	}
	// ���������߳�
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();        //��Ҫִ��һ���̺߳���
		idleThreadSize_++;           // ��¼��ʼ�����̵߳�����
	}
}

//�����̺߳���          �̳߳�����һ���̣߳������̺߳������̳߳ظ��߳�ָ��
// �̳߳������̴߳��������������������
void ThreadPool::threadFunc(int threadid)     
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	// �����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ        
	for (;;)
	{
		std::shared_ptr<Task> task;
		{                              // ������������Ϊ����С��������
			// ���Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id() << "���Ի�ȡ����..." << std::endl;


			// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳�
			// �������յ�������initThreadSize_�������߳�Ҫ���л��գ�
			// ��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s

			while (taskQue_.size() == 0)     // ���������û�������ʱ��
			{

				if (!isPoolRunning_)             // ȷ���û��ύ������ɺ����˳�
				{
					threads_.erase(threadid); // std::this_thread::getid()
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
					exitCond_.notify_all();    // 
					return;              // �����̺߳��������ǽ�����ǰ�߳��ˣ�������������������
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
		
					// ����������ʱ������
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳������������ر���Ҫ�޸�
							// ���̶߳�����߳��б�������ɾ��     ��û�а취��threadFunc <==> thread������thread_���б����У�       vector==��>map�������׹���ɾ��
							// threadId  ==> thread����  ===�� ɾ��
							threads_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << " exit!"
								<< std::endl;

							return;
						}
					}
				}
				else
				{
					// �� �ȴ��������notEmpty()����
					notEmpty_.wait(lock);
				}

				// �̳߳�Ҫ�����������߳���Դ
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadid); // std::this_thread::getid()
				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!"<< std::endl;
				//	exitCond_.notify_all();    // 
				//	return;              // �����̺߳��������ǽ�����ǰ�߳���!
				//}
			}


			idleThreadSize_--;    

			std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�..." << std::endl;

			// �� ���������ȡ��һ���������
			//auto task = taskQue_.front();
			//std::shared_ptr<Task> task = taskQue_.front();
			task = taskQue_.front();
			taskQue_.pop();                 // pop() ����ȷ����Ԫ�ص��ʵ��������ڹ�������ִ����������
			taskSize_--;

			// �����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// ȡ��һ�����񣬽���֪ͨ��֪ͨ���Լ����ύ����������
			notFull_.notify_all();

		} // ���������򣬰����ͷŵ�

		if (task != nullptr)
		{
			// �ܵ�ǰ����̸߳���ִ���������
			//task->run();
			task->exec();

		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
	}  // ���������� task Ҳ������

}
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


///////////////  �̷߳���ʵ��
int Thread::generateId_ = 0;

//���캯��
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)      // �߳�id++
{

}

// ��������
Thread::~Thread()
{

}

//�����߳�
void Thread::start()
{
	// ����һ���߳���ִ��һ���̺߳��� 
	std::thread t(func_,threadId_);  // C++11��˵�� �̶߳���t ���̺߳���func_
	t.detach();            // ����Ϊ�����߳�                        ������detach()����������ζ���´������߳̽���std::thread����t�з��룬����������ִ��

	// ���������ţ���Ȼt���std::thread���������ˣ����������Ѿ������룬���Եײ���߳���Ȼ�����ִ�С�t�����ٲ���Ӱ�쵽�Ѿ�������̵߳�ִ��
}

int Thread::getId()const
{
	return threadId_;
}

/////////////////  Task����ʵ��
Task::Task()
	: result_(nullptr)
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); // ���﷢����̬����
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

/////////////////   Result������ʵ��
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get() // �û����õ�
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task�������û��ִ���꣬����������û����߳�
	return std::move(any_);
}

void Result::setVal(Any any)  // ˭���õ��أ�����
{
	// �洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post(); // �Ѿ���ȡ������ķ���ֵ�������ź�����Դ
}

