#pragma once

#include <vector>
#include <string>

/* 网络库底层的缓冲区的类型定义 */
class Buffer{
public:
    static const std::size_t kCheapPrepend = 8;
    static const std::size_t kInitialSize  = 1024;

    explicit Buffer(std::size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readIndex_(kCheapPrepend)
        , writeIndex_(kCheapPrepend)
        {}

    std::size_t readableBytes() const { return writeIndex_ - readIndex_; }
    std::size_t writeableBytes() const { return buffer_.size() - writeIndex_; }
    std::size_t prependableBytes()  const { return readIndex_; } 

    /* 返回缓冲区中可读数据的起始地址 */
    const char* peek() const {
         return begin() + readIndex_;
    }

    void retrieve(std::size_t len){
        if(len < readableBytes()){
            readIndex_ += len;  /* 应用只读取了刻度缓冲区数据的一部分，就是len， */
        }
        else{       /* len == readableBytes() */
            retrieveAll();
        }
    }

    void retrieveAll(){
        readIndex_ = kCheapPrepend;
        writeIndex_ = kCheapPrepend;
    }

    /* 把onMessage函数上报的Buffer数据，转换成String类型的数据返回 */
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes()); /* 应用可读取数据的长度 */ 
    }

    std::string retrieveAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);   /* 上面一句已经把缓冲区中可读的数据读取出来，这里是对缓冲区进行复位的操作 */
        return result;
    }


    
    /* len > buffer.size() - writeIndex_ */
    void ensureWritableBytes(size_t len){
        if(writeableBytes() < len){
            /* 扩容 */
            makeSpace(len);
        }
    }

    /* 扩容函数 */
    void makeSpace(size_t len){
        if( writeableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writeIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readIndex_,
                      begin() + writeIndex_,
                      begin() + kCheapPrepend);
            readIndex_ = kCheapPrepend;
            writeIndex_ = readIndex_ + readable;
        }
    }

    /* 把[data , data + len] 内存上的数据添加到writeable缓冲区中 */
    void append(const char* data, size_t len){
        ensureWritableBytes(len);
        std::copy(data,data + len, beginwrite());
        writeIndex_ += len;
    }

    /* 从fd上读取数据 */
    ssize_t readFd(int fd, int* savedErrno);
    /* 通过fd发送数据 */
    ssize_t writeFd(int fd, int* savedErrno);

private:
    char* begin(){ return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    char* beginwrite() { return begin() + writeIndex_; }
    const char* beginwrite() const  { return begin() + writeIndex_; }

    std::vector<char> buffer_;
    std::size_t readIndex_;
    std::size_t writeIndex_;
};