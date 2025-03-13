#include "Acceptor.h"
#include <sys/types.h>          
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "InetAddress.h"

static int createNonBlocking(){
    int sockfd = ::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if( sockfd < 0 ){
        LOG_FATAL("%s:%s:%dlisten socket error: %d \n", __FILE__,__FUNCTION__ , __LINE__ , errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listendAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonBlocking())            /* 创建套接字 */
    , acceptChannel_(loop,acceptSocket_.fd())
    , listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listendAddr);  /* 绑定套接字 */
    /* TcpServer::start() => 执行 Acceptor.listen 有新用户连接，要执行一个回调(connfd => channel => subloop)*/
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}
void Acceptor::listen(){
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

/* listenfd 有事件发生了，就是有新用户连接了 */
void Acceptor::handleRead(){
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);
        }
        else{
            ::close(connfd);
        }
    }
    else{
        LOG_ERROR("%s:%s:%dlisten socket error: %d \n", __FILE__,__FUNCTION__ , __LINE__ , errno);
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reached limit: %d \n", __FILE__,__FUNCTION__ , __LINE__ , errno);
        }

    }
}