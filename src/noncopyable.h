#pragma once

namespace tinytcp {


// 不可拷贝对象的封装
class Noncopyable {

public:
    Noncopyable() = default;
    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

    
} // namespace tinytcp


