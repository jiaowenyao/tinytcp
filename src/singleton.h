#pragma once

#include <memory>

namespace tinytcp {

template<class T, class X = void, int N = 0>
class Singleton {
public:
    static T* get_instance() {
        static T v;
        return &v;
    }
private:
};

template<class T, class X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> get_instance() {
        static std::shared_ptr<T> v(new T);
        return v;
    }

};

} // namespace tinytcp
