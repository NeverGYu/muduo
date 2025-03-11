#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>
#include <mutex>

/* 防止一个线程创建多个线程 */
__thread EventLoop* t_loopInThisThread = nullptr;

/* 定义默认的Poller IO复用的超时时间 */
const int kPollTimeMs = 10000;

/* 创建eventfd，用来唤醒subReactor处理新来的channel */
int createEventfd(){
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0 ){
        LOG_FATAL("eventfd err:%d\n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this,threadId_);
    if(t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else{
        t_loopInThisThread = this;
    }

    /* 设置 wakeupfd 的事件类型以及发生事件的回调操作 */
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::hanldeRead,this));
    /* 每一个 eventloop 都将监听 wakeupchannel 的 EPOLLIN 读事件*/
     wakeupChannel_->enableReading();

}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
    t_loopInThisThread = nullptr;
}


void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping",this);
    while(!quit_){
        activeChannels_.clear();
        /* 监听两类fd: clientfd 和 wakeupfd */
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel* channel : activeChannels_){
             /* Poller 监听哪些 channel 发生事件了，然后上报给 EventLoop ，通知 channel 处理相应的事件 */
            channel->handleEvent(pollReturnTime_);
        }

        /* 执行当前 EventLoop 事件循环需要处理的回调操作 */
        /* 解析
         * IO线程(mainLoop): accept => fd => channel => subloop
         *  mainloop 事先注册一个回调函数 => subloop 来执行 : wakeup(eventfd) => 执行之前mainloop注册的cb操作 
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping \n",this);
    looping_ = false;
}   


/* 退出事件循环 1. loop在自己的线程调用quit 2. 在非loop的线程中，调用loop的quit */
void EventLoop::quit(){
    quit_ = true;

    /* 如果在其他线程中调用 quit: 在一个 subloop(worker) 中，调用了 mainloop(IO) 的quit */ 
    if(!isInLoopThread()){  
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){   /* 在当前loop线程中，执行cb */
        cb();
    }
    else{
        queueInLoop(cb);    /* 在非当前loop线程中，就需要唤醒loop所在的线程，执行cb */
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    /* 唤醒相应的，需要执行上面回调操作的loop的线程了 */
    /* || callingPendingFunctors_ 的意思是：当前loop正在执行回调，但是loop又有了新的回调 */
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); /* 唤醒 loop 所在线程 */ 
    }
    
}

void EventLoop::hanldeRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one, sizeof one);
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::handleRead() read %d bytes instead of 8 \n", static_cast<int>(n));
    }
}

 /* 用来唤醒loop所在的线程 => 向 wakeupfd_写入一个数据, wakeupChannel 就发生读事件，当前 loop 线程就会被唤醒 */
void  EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof(one));
    if( n != sizeof(one)){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n",n);
    }
}        
    
/* EventLoop 方法 =》 Poller 方法 */
void  EventLoop::updateChannel(Channel* channel){
    poller_->updateChannel(channel);
}

void  EventLoop::removeChannel(Channel* channel){
    poller_->removeChannel(channel);
}

bool  EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}

 /* 执行回调 */
void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> Lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for( const Functor& functor : functors ){
        functor();  /* 执行当前loop需要执行的回调 */
    }
    callingPendingFunctors_ = false;
} 