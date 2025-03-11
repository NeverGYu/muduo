#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <iostream>
#include <functional>

using namespace muduo;
using namespace muduo::net;
using namespace std;

/* 基于muduo开发网络服务器程序
1. 组合TcpServer对象
2. 创建Eventloop对象的指针
3. 明确Tcpserver构造函数需要什么参数，输出ChatServer的构造函数
4. 在当前服务器类的构造函数里，注册处理连接的回调函数和处理时间的回调函数
5. 设置合适的服务端线程数量，muduo库会自己分配I/O线程和工作线程
*/
class Chatserver
{
public:
    Chatserver(EventLoop* loop, // 事件循环，Reactor 反应堆
               const InetAddress& listenAddr, // IP+Port
                const string& nameArg)// 服务器的名字
                :_server(loop,listenAddr,nameArg)
                {
                    // 给服务器注册用户连接的创建和断开的回调
                    _server.setConnectionCallback(std::bind(&Chatserver::onConnection,this,placeholders::_1));
                    // 给服务器注册用户读写事件的回调
                    _server.setMessageCallback(std::bind(&Chatserver::onMessage,this,placeholders::_1,placeholders::_2,placeholders::_3));
                    // 设置服务器的线程数量
                    _server.setThreadNum(4);
                }

    void start(){
        _server.start();
    }

private:
    // 专门处理用户连接和断开
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << "state : online" <<endl;
        }
        else{
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << "state : offline" <<endl;
            conn->shutdown(); // close(fd)
            // _loop
        }
    }

    // 专门处理用户读写事件
    void onMessage(const TcpConnectionPtr& conn, // 连接
        Buffer* buf, // 缓冲区
        Timestamp time)   // 接收到数据的事件信息
    {
          string buffer = buf->retrieveAllAsString();
          cout << "recv data: " << buffer << "time: " << time.toString() << endl;
          conn->send(buffer);
    }

    TcpServer _server; //#1
    EventLoop * loop;   //#2 epoll

};

// int main(){
//     EventLoop loop;
//     InetAddress addr("127.0.0.1",6001);
//     Chatserver server(&loop,addr,"ChatServer");

//     server.start();
//     loop.loop(); // epoll_wait以阻塞方式等待用户连接、以连接用户的等待事件
// }
