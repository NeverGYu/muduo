#pragma once

#include "nocopyable.h"
#include "Timestamp.h"
#include "Channel.h"
#include <vector>
#include <unordered_map>
class EventLoop;

/* muduo 库中多路事件分发器的核心IO复用模块 */
class Poller{
public:
    using ChannelList = std::vector<Channel*>;
    using sockfd = int;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    /* 给所有IO复用保留统一的接口 */
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
   
    /* 判断参数channel是否在当前poller中 */
    virtual bool hasChannel(Channel* channel) const;

    /* EventLoop 可以通过该接口获取默认的IO复用的具体实现 */
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<sockfd,Channel*>;
    ChannelMap channels;
private:
    EventLoop* ownerLoop_; /* 定义事件所属的事件循环 */
};