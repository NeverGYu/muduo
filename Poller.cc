#include "Poller.h"

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop){
}

bool Poller::hasChannel(Channel* channel) const{
    ChannelMap::const_iterator it = channels.find(channel->fd());
    return it != channels.end() && it->second == channel;
}