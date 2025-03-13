#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
 * 从 fd 上读取数据， Poller工作在LT模式
 * Buffer 缓冲区是由大小的，但是从 fd 上读取数据时，却不知道tcp数据最终的大小
 */
ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65535] = {0};  /* 栈上内存空间:64KB  */
    struct iovec vec[2];
    const size_t writeable  = writeableBytes();  /* 这是 buffer 底层缓冲区剩余的可写空间大小 */
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writeable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf); 
       
    const int iovcnt = (writeable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *savedErrno = errno;
    }
    else if (n <= writeable)
    {
        writeIndex_ += n;
    }
    else { /* extrabuf 也写入了数据 */
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writeable);
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    ssize_t n =  write(fd,peek(),readableBytes());
    if( n < 0){
        *savedErrno = errno;
    }
    return n;
}