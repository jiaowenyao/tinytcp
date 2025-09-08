#pragma once

#include <memory>
#include <functional>
#include <set>
#include <vector>
#include "mutex.h"
#include "src/net/net_err.h"


namespace tinytcp {
class TimerManager;
class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    using ptr = std::shared_ptr<Timer>;
    bool cancel();
    bool refresh();
    bool reset(uint64_t ms, bool from_now);

private:
    Timer(uint64_t ms, std::function<net_err_t()> cb, bool recurring, TimerManager* manager);
    Timer(uint64_t next);

private:
    bool m_recurring = false; // 是否循环定时器
    uint64_t m_ms = 0;        // 执行周期
    uint64_t m_next = 0;      // 精确的执行时间(当前时间加上需要执行的时间)
    std::function<net_err_t()> m_cb;
    TimerManager* m_manager = nullptr;

private:
    struct Comparator {
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
    };

};

class TimerManager {
friend class Timer;

public:
    using RWMutexType = RWMutex;
    using ptr = std::shared_ptr<TimerManager>;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr add_timer(uint64_t ms, std::function<net_err_t()> cb, bool recurring = false);

    // 用weak_ptr当条件，有一个引用计数，如果已经消失了，那么说明条件已经不满足了，就不用执行了
    Timer::ptr add_condition_timer(uint64_t ms, std::function<net_err_t()> cb,
                                   std::weak_ptr<void> weak_cond,
                                   bool recurring = false);
    
    uint64_t get_next_time();
    void list_expired_cb(std::vector<std::function<net_err_t()> >& cbs); // 返回已经超时了的，需要执行的回调函数
protected:
    virtual void on_timer_inserted_at_front() = 0;
    void add_timer(Timer::ptr val, RWMutexType::WriteLock& lock);
    bool has_timer();
private:
    // 检测是否调整了系统时间
    bool detect_clock_roll_over(uint64_t now_ms);
private:
    RWMutexType m_mutex;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    bool m_tickled = false;
    uint64_t m_previous_time = 0;
};

} // namespace tinytcp


