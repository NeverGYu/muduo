#include "InetAddress.h"

InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_,sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    inet_pton(AF_INET,ip.c_str(),&addr_.sin_addr);
}

std::string InetAddress::toIp() const{
    /* 打印ip */ 
    char buf[64] = {0};
    inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const{
    /* 打印ip 和 端口*/ 
    char buf[64] = {0};
    inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u",port);
    return buf;
}

uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}

#include <iostream>

int main(){
    InetAddress addr(8080);
    std::cout << addr.toPort() << std::endl;
    return 0;
}