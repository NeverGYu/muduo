#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "errno.h"

#include <unistd.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>


static EventLoop* CheckLoopNotNull(EventLoop* loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d TcpConnection is null !\n", __FILE__,__FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
    const std::string& name,
    int sockfd,
    const InetAddress& localAddr,
    const InetAddress& peerAddr) 
    : loop_(CheckLoopNotNull(loop))
    , name_(name)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)
{
    /* 下面给channel设置相应的回调函数，pollor 通知channel 感兴趣的的事件发生了，channel就调用回调操作函数 */
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWirte,this));
    channel_->setCloseCallbak(
        std::bind(&TcpConnection::handleClose,this)); 
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleClose,this)); 
    LOG_INFO("TcpConnection::ctor[%s] at fd = %d \n",name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd = %d state = %d\n",name_.c_str(), socket_->fd(), state_.load());
}

void TcpConnection::send(const std::string& buf){
    if( state_ == kConnected ){
        if( loop_->isInLoopThread() )
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
                ));
        }
    }
}

/* 发送数据函数
 * 应用写的快，内核发送数据慢，需要把待发送数据写入到缓冲区，而且设置了水位回调
*/
void TcpConnection::sendInLoop(const void* message,size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    /* 之前调用过该connection的shutdown，不能再进行发送 */
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected,give up writing!");
    }

    /* 表示 channel_ 第一次写数据，而且缓冲区没有待发送数据 */
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(),message,len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                /* 数据一次性发送完成，就不用再给channel设置epollout 事件  */
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this()));
            }
            
        }
        else
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if( errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
        
    }
    /* 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区
     * 然后给channel 注册epollout事件，Poller发现tcp的发送缓冲区有空间，就会通知socket(channel)调用writeCallback回调方法
     * 也就是调用TcpConnection::handleWrite方法，把发送缓冲区的数据全部发送完成
     */
    if (!faultError && remaining > 0)   
    {
        /* 目前发送缓冲区剩余待发送数据的长度 */
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_
           && oldLen < highWaterMark_
           && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_,shared_from_this(),oldLen + remaining));    
        }
        outputBuffer_.append((char*)message + nwrote, remaining);

        if( !channel_->isWriting()){
            channel_->enableWriting(); /* 这里一定要注册channel的写事件，否则poller不会像channel通知epollout事件 */ 
        }
    }
}

void TcpConnection::connectEstablished(){
    setState(kConnected);
    channel_->tie(shared_from_this()); /* 记录TcpConnection存活的状态 */
    channel_->enableReading();   /* 向poller注册channel的读事件 */

    /* 新连接建立，执行回调 */
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed(){
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::shutdown(){
    if (state_ == kConnected)
    {
       setState(kDisconnecting);
       loop_->runInLoop(std::bind(&TcpConnection::shutdownInloop,this));
    }
}

void TcpConnection::shutdownInloop(){
   if (!channel_->isWriting())
   {
        socket_->shutdownWrite();
   }
}

void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0){
        /* 已建立连接 的用户，由可读事件发生了，调用用户传入的回调操作onMessage */
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0){
        handleClose();
    }
    else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleClose();
    }
}

void TcpConnection::handleWirte(){
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0){
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    /* 唤醒loop_对应的thread线程，执行回调 */
                    loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInloop();
                }
            }
        }
        else{
            LOG_ERROR("TcpConnection::handleWirte");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd = %d is down, no more writing \n", channel_->fd());
    }
    
}

/* poller => channel::closeCallbak => TcpConnection::handleClose */
void TcpConnection::handleClose(){
    LOG_INFO("fd = %d state = %d \n",channel_->fd(),state_.load());
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);  /* 关闭连接的回调，是TcpServer::removeConnection 回调方法*/
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0){
        err = errno;
    }
    else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}