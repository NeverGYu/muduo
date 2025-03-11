#include "Channel.h"
#include "sys/epoll.h"
#include "EventLoop.h"


// Define LOG_INFO macro if not defined
#ifndef LOG_INFO
#define LOG_INFO(fmt, ...) printf(fmt, ##__VA_ARGS__)
#endif

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

/* EventLoop ：ChannelList Poller */
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{

}

Channel::~Channel(){

}

/* Channel 的 tie 方法什么时候调用过？ */
void Channel::tie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}

/* 当改变 Channel 所表示 fd 的 events 事件后，update 负责在 poller 里面改变 fd 相应的事件 epoll_ctl */
void Channel::update(){
    /* 通过 Channel 所属的 EventLoop ，调用 PPoller 的相应方法，注册fd的events事件 */
    // add code
    // loop_->updateChannel(this);
}

/* 在 Channel 所属的 EventLoop 中，把当前的 Channel 删除掉 */
void Channel::remove(){
    // add code
    // loop_->removeChannel();
}

void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

/* 根据Poller监听的Channel发生的具体事件，由Channel负责调用具体的回调函数 */
void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("Channel handleEvent revents:%d\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN|EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}
