#pragma once

#include "nocopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * 
 * => TcpConnection 设置相应的回调函数 => Channel => Poller => 监听到事件就会调用 Channel 的回调函数
 */
class TcpConnection : nocopyable , public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddr() const { return localAddr_; }
    const InetAddress& peerAddr() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }  
    
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }
   
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }
   
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }
   
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }
   
    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }
    
    void connectEstablished();
    void connectDestroyed();

    void handleRead(Timestamp receiveTime);
    void handleWirte();
    void handleClose();
    void handleError();

    void send(const std::string &buf);

    void shutdown();
    

private:
    enum StateE{
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };

    void sendInLoop(const void* message,size_t len);

    void shutdownInloop();

    void setState(StateE state){ state_ = state; } 

    EventLoop* loop_;           /* 这里绝对不是baseLoop，因为TcpServer 都是在subloop里面管理的 */
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    /* 这里和Acceptor类似   Acceptor => mainLoop  TcpConnection => subLoop */
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;    /* 接受数据的缓冲区 */
    Buffer outputBuffer_;   /* 发送数据的缓冲区 */
};