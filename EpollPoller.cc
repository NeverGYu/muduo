#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include <errno.h>
#include <unistd.h>

/* channel 未添加到 poller 中 */
const int kNew = -1;
/* channel 已添加到 poller 中 */
const int kAdded = 1;
/* channel 从 poller 中删除 */
const int kDeleted = 2;

EpollPoller::EpollPoller(EventLoop* loop) 
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if(epollfd_ < 0){
        LOG_FATAL("epoll_create error: %d",errno);
    }
}

EpollPoller::~EpollPoller() {
    ::close(epollfd_);
}

/* channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel 
 *
 *                  EventLoop    =>  poller.poll
 *        ChannelList(all)             Poller
 *                             ChannelMap<fd, channel*>
 */
void EpollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();  /* 获取channel在Poller中当前的状态 */
    LOG_INFO("func = %s => fd = %d events = %d index = %d \n",__FUNCTION__,channel->fd(), channel->events(), index);

    if(index == kNew || index == kDeleted){
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else{
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
        
    }


}

/* 从 poller 中删除 channel */
void EpollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func = %s => fd = %d \n",__FUNCTION__,channel->fd());
    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

Timestamp EpollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    /* 使用LOG_DEBUG 更合理 */
    LOG_INFO("func=%s => fd total count:%d\n",__FUNCTION__,static_cast<int>(channels_.size()));  
    int numEvents = epoll_wait(epollfd_, &*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;  /* errno 是全局变量，可被多线程修改，故需要保存 */
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        LOG_INFO("%d events happened\n",numEvents);
        fillactiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size()){
            events_.resize(events_.size() * 2 );
        }
    }
    else if(numEvents == 0){
        LOG_DEBUG("%s timeout! \n",__FUNCTION__);
    }
    else{
        if( saveErrno != EINTR ){
            errno  = saveErrno;
            LOG_ERROR("EPollPoller::poll err!");        
        }
    }
    return now;
}

void EpollPoller::fillactiveChannels(int numevents,ChannelList* activeChannels) const {
    for(int i =0 ; i < numevents; i++){
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); /* EventLoop 就拿到了它的poller给它返回的所有发生事件的 channel 列表 */
    }
} 

/* epoll add/mod/del 操作 */
void EpollPoller::update(int operation, Channel* channel) {
    epoll_event event;
    bzero(&event,sizeof(event));
    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    
    if(epoll_ctl(epollfd_,operation,fd,&event) < 0 ){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d",errno);
        }
        else{
            LOG_FATAL("epoll_ctl add/mod error:%d",errno);
        }
    }
}                       
