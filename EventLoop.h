#pragma once

#include "nocopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>


class Channel;
class Poller;

/* 事件循环类：主要包含两个模块：Channel 和 Poller(epoll的抽象) */
class EventLoop : nocopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();    /* 开始事件循环 */
    void quit();    /* 退出事件循环 */

    Timestamp pollReturnTime() const { return pollReturnTime_; }
    void runInLoop(Functor cb);    /* 在当前loop中执行回调函数 */ 
    void queueInLoop(Functor cb);  /* 把回调函数放到队列中，唤醒loop所在的线程，执行cb */
    void wakeup();                 /* 用来唤醒loop所在的线程 */
    
    /* EventLoop 方法 =》 Poller 方法 */
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    bool isInLoopThread() const{ return threadId_ == CurrentThread::tid(); }  /* 判断EventLoop对象是否在自己的线程中 */
private:
    void hanldeRead();          /* wake up */
    void doPendingFunctors();   /* 执行回调 */
     
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;                 /* 原子操作，通过CAS实现的 */
    std::atomic_bool quit_;                    /* 标志退出loop循环 */
    const pid_t threadId_;                     /* 记录当前loop所在的进程ID */
    Timestamp pollReturnTime_;                 /* poller 返回发生事件的channels的时间点 */
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;    /* 主要作用：当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员变量唤醒subloop处理这个channel*/
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;  /* 标识当前的loop是否需要执行的回调操作 */
    std::vector<Functor>pendingFunctors_;      /* 存储loop需要执行的所有回调操作 */
    std::mutex mutex_;                         /*  互斥锁，用来保护上面vector容器的线程安全操作 */
};