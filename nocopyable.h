#pragma once
/*
*   nocopyable 被继承之后，派生类对象可以正常构造和析构，但是派生类
*   对象无法进行拷贝构造和赋值运算
*/

class nocopyable{
public:
    nocopyable(const nocopyable&) = delete;
    nocopyable& operator=(const nocopyable&) = delete;
protected:
    nocopyable() = default;
    ~nocopyable() = default;
};