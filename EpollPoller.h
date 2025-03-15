#pragma once

#include "Poller.h"
#include "Timestamp.h"
#include <vector>
#include <sys/epoll.h>

class Channel;

/*
 * epoll 的使用
 * epoll_create
 * epoll_ctl
 * epoll_wait
*/
class EpollPoller : public Poller{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;
    
    /* 重写基类Poller的抽象方法 */
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    /* 辅助函数 */
    void fillactiveChannels(int numevents,ChannelList* activeChannels) const;  /* 填写活跃连接 */
    void update(int operation, Channel* Channel);                          /* 更新Channel通道 */

    using EventList = std::vector<epoll_event>;
    int epollfd_;
    EventList events_;
};