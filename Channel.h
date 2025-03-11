#pragma once

#include "nocopyable.h"
#include <functional>
#include "Timestamp.h"
#include <memory>
#include "Logger.h"

class EventLoop;
class Timestamp;

/* 
 * 理清楚：EventLoop、Poller、Channel 之间的关系  《Reactor 模型对应 多路事件分发器》
 * Channel 理解为通道，封装了sockfd 和 其他感兴趣的事件，如 EPOLLIN 和 EPOLLOUT 事件 还绑定了Poller返回的具体事件
*/
class Channel : nocopyable{
public:
    using EventCallback =  std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

     /* fd 得到 poller 通知以后，处理事件的 */
    void handleEvent(Timestamp receiveTime);

    /* 设置回调函数对象 */
    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb){ writeCallback_ = std::move(cb); }
    void setCloseCallbak(EventCallback cb){ closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb){ errorCallback_ = std::move(cb); }

    /* 防止当 Channle 被手动 remove , Channel 还在执行回调操作 */
    void tie(const std::shared_ptr<void>&);

    /* 返回文件描述符 */
    int fd() const { return fd_;}
    /* 返回感兴趣的事件 */
    int events() const { return events_; }
    /* Poller 通过函数接口来设置就绪事件 */
    int set_revents(int revt) { revents_ = revt; return revents_;}
    

    /* 设置 fd 相应的状态 */
    void enableReading() { events_ |= kReadEvent; update();}
    void disableReading() { events_ &= ~kReadEvent;update();}
    void enableWriting(){ events_ |= kWriteEvent; update();}
    void disableWriting() { events_ &= ~kWriteEvent;update();}
    void disableAll() {events_ = kNoneEvent;update();};

    /* 返回 fd 当前的状态 */
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    int index() const { return index_; }
    void set_index(int idx){ index_ = idx; }

    /* one loop per thread */
    EventLoop* ownerloop() { return loop_; }
    void remove();
private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; /* 事件循环 */
    const int fd_;     /* Poller 监听的对象 */
    int events_;      /* 注册 fd 感兴趣的事件 */
    int revents_;     /* Poller 返回具体发生的事件 */
    int index_;       /* 被 Poller 使用 */

    std::weak_ptr<void> tie_;
    bool tied_;
    
    /* 因为 Channel 通道里面能够获知 fd 最终发生的具体的事件revents，所以它负责调用具体事件的回调操作 */
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};