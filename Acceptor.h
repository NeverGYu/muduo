#pragma once

#include "nocopyable.h"
#include "Socket.h"
#include "Channel.h"
#include <functional>


class EventLoop;
class InetAddress;

class Acceptor : nocopyable{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listendAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb){newConnectionCallback_ = std::move(cb);}

    bool listening() const { return listening_; }
    void listen();
private:
    void handleRead();
     
    EventLoop* loop_; /* mainLoop */
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};