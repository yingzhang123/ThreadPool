#include"threadpool.h"
#include<functional>
#include<thread>
#include<iostream>
#include<chrono>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 100;     
const int THREAD_MAX_IDLE_TIME = 6;    // 单位:秒

// 线程池构造
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

// 线程池析构
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;

	// 等待线程池里面所有的线程返回  有两种状态：阻塞 & 正在执行任务中
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}


// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
	{
		return;
	}
	poolMode_ = mode;
}

// 设置task任务队列的上限阈值
void ThreadPool::setTaskQueMaxThreshHold(size_t threshold)
{
	if (checkRunningState())     // 不允许运行后，再设置了
	{
		return;
	}
	taskQueMaxThreshHold_ = threshold;
}

// 设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHoid(int threshold)
{
	if (checkRunningState())     // 不允许运行后，再设置了
	{
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThresHold_ = threshold;
	}
}

// 给线程池提交任务    用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// ①获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	// ②线程的通信   等待任务队列有空余
	//while (taskQue_.size() == taskQueMaxThreshHold_)
	//{
	//	notFull_.wait(lock);   // 1、释放当前锁  2、进程进入等待状态
	//}
	
	//队列大小小于最大阈值时，返回true，允许线程继续执行。如果谓词返回false，则线程会再次进入等待状态。
	// 这个重载版本的wait()内部实际上是一个循环，它会在虚假唤醒发生后重新检查条件，确保只有在条件真正满足时线程才会继续执行
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	// wait()  这个函数会一直等待，直到另一个线程调用条件变量的 notify_one() 或 notify_all() 函数为止。在等待期间，当前线程会释放锁，并阻塞在条件变量上。当条件满足时，wait() 函数返回，并重新获取锁。 
	// wait_for()  函数会等待一段指定的时间，如果在指定的时间内条件变量没有被满足，函数会返回。这种等待是有限的，不会一直阻塞线程。如果条件在超时前被满足，函数会提前返回
	//wait_until()==》接受一个绝对时间点作为参数，而不是一个相对时间段。它会等待直到指定的时间点，或者直到条件被满足。
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }))         // wait()    wait_for()   wait_until()==》接受一个绝对时间点作为参数，而不是一个相对时间段。它会等待直到指定的时间点，或者直到条件被满足。
	{
		// 表示notFull_等待1s钟，条件依然不满足
		std::cerr << "task queue is full,submit task fail." << std::endl;
		// return task->getResult();                         // Task Result 线程执行完task,task对象就析构掉了，所以不能采用这种方式
		return Result(sp,false);         //
	}


	// ③如果有空余， 把任务放入任务队列中
	taskQue_.emplace(sp);
	taskSize_++;

	// ④因为放了新的任务，任务队列肯定不空了，在notEmpty_上通知,赶快分配线程执行任务
	notEmpty_.notify_all();

	// 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
	// cached模式  任务处理计较紧急  场景：小而快的任务        时间长的任务不太时候使用cached模式，因为当任务过多的时候会创建过多的线程（耗时的任务会长时间占有线程），导致系统崩溃
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThresHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// 创建新线程
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));       // std::make_unique是C++14中引入的一个函数模板
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		// 启动线程
		threads_[threadId]->start();
		// 修改线程个数相关的变量
		curThreadSize_++;
		idleThreadSize_++;
		/*threads_.emplace_back(std::move(ptr));*/
	}

	// 返回任务的Result对象
	return Result(sp);

}

// 开启线程池
void ThreadPool::start(size_t initThreadSize)
{
	// 设置线程池的运行状态
	isPoolRunning_ = true;
	// 记录初始线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象(还每启动线程)
	for (int i = 0; i < initThreadSize_; ++i)
	{
		// 创建thread线程对象的时候，把线程函数给到thread对象        线程对象执行什么函数由线程池对象决定
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));           // 使用Thread裸指针管理对象    

		// 使用unique_ptr智能指针管理Thread对象   ===》   直接使用new可能导致错误，如忘记调用delete。使用std::unique_ptr可以自动管理内存，防止这类错误
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));       // std::make_unique是C++14中引入的一个函数模板
		//threads_.emplace_back(std::move(ptr));  // vector
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));   //map
	}
	// 启动所有线程
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threads_[i]->start();        //需要执行一个线程函数
		idleThreadSize_++;           // 记录初始空闲线程的数量
	}
}

//定义线程函数          线程池启动一个线程，具体线程函数由线程池给线程指定
// 线程池所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadid)     
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	// 所有任务必须执行完成，线程池才可以回收所有线程资源        
	for (;;)
	{
		std::shared_ptr<Task> task;
		{                              // 添加这个作用域，为了缩小锁的粒度
			// ①先获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;


			// cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，应该把多余的线程
			// 结束回收掉（超过initThreadSize_数量的线程要进行回收）
			// 当前时间 - 上一次线程执行的时间 > 60s

			while (taskQue_.size() == 0)     // 当任务队列没有任务的时候
			{

				if (!isPoolRunning_)             // 确保用户提交任务完成后，再退出
				{
					threads_.erase(threadid); // std::this_thread::getid()
					std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
					exitCond_.notify_all();    // 
					return;              // 结束线程函数，就是结束当前线程了！！！！！！！！！！
				}

				if (poolMode_ == PoolMode::MODE_CACHED)
				{
		
					// 条件变量超时返回了
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							// 开始回收当前线程
							// 记录线程相关数量的相关变量要修改
							// 把线程对象从线程列表容器中删除     （没有办法）threadFunc <==> thread对象（在thread_队列变量中）       vector==》>map更加容易管理，删除
							// threadId  ==> thread队像  ===》 删除
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
					// ② 等待任务队列notEmpty()条件
					notEmpty_.wait(lock);
				}

				// 线程池要结束，回收线程资源
				//if (!isPoolRunning_)
				//{
				//	threads_.erase(threadid); // std::this_thread::getid()
				//	std::cout << "threadid:" << std::this_thread::get_id() << " exit!"<< std::endl;
				//	exitCond_.notify_all();    // 
				//	return;              // 结束线程函数，就是结束当前线程了!
				//}
			}


			idleThreadSize_--;    

			std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功..." << std::endl;

			// ③ 从任务队列取出一个任务出来
			//auto task = taskQue_.front();
			//std::shared_ptr<Task> task = taskQue_.front();
			task = taskQue_.front();
			taskQue_.pop();                 // pop() 方法确保了元素的适当生命周期管理，包括执行析构函数
			taskSize_--;

			// 如果依然有剩余任务，继续通知其他线程执行任务
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			// 取出一个任务，进行通知，通知可以继续提交生产任务了
			notFull_.notify_all();

		} // 出了作用域，把锁释放掉

		if (task != nullptr)
		{
			// ④当前这个线程负责执行这个任务
			//task->run();
			task->exec();

		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
	}  // 出现作用域 task 也会析构

}
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}


///////////////  线程方法实现
int Thread::generateId_ = 0;

//构造函数
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)      // 线程id++
{

}

// 析构函数
Thread::~Thread()
{

}

//启动线程
void Thread::start()
{
	// 创建一个线程来执行一个线程函数 
	std::thread t(func_,threadId_);  // C++11来说， 线程对象t 和线程函数func_
	t.detach();            // 设置为分离线程                        调用了detach()方法，这意味着新创建的线程将从std::thread对象t中分离，允许它独立执行

	// 出了右括号，虽然t这个std::thread对象被销毁了，但由于它已经被分离，所以底层的线程仍然会继续执行。t的销毁不会影响到已经分离的线程的执行
}

int Thread::getId()const
{
	return threadId_;
}

/////////////////  Task方法实现
Task::Task()
	: result_(nullptr)
{}

void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run()); // 这里发生多态调用
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

/////////////////   Result方法的实现
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get() // 用户调用的
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait(); // task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(any_);
}

void Result::setVal(Any any)  // 谁调用的呢？？？
{
	// 存储task的返回值
	this->any_ = std::move(any);
	sem_.post(); // 已经获取的任务的返回值，增加信号量资源
}

