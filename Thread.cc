#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>

std::atomic_int numCreated_{0};

Thread::Thread(ThreadFunc func_, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func_))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach();  /* thread 类提供的分离线程的方法 */ 
    }
}

/* 一个 thread 记录的就是一个新线程的信息 */
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    /* 开启线程 */
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();  /* 获取线程的tid值 */
        sem_post(&sem);
        func_();                      /* 开启一个新线程，专门执行该线程函数 */
    }));
    
    /* 这里必须获取新创建的线程的tid值 */
    sem_wait(&sem);
}

void Thread::join(){
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread %d",num);
        name_ = buf;
    }
}
