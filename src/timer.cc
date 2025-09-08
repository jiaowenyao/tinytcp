#include "timer.h"
#include "util.h"


namespace tinytcp {

bool Timer::Comparator::operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const {
    if (!lhs && !rhs) {
        return false;
    }
    if (!lhs) {
        return true;
    }
    if (!rhs) {
        return false;
    }
    if (lhs->m_next < rhs->m_next) {
        return true;
    }
    if (rhs->m_next < lhs->m_next) {
        return false;
    }
    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms, std::function<net_err_t()> cb, bool recurring, TimerManager* manager)
    : m_recurring(recurring)
    , m_ms(ms)
    , m_cb(cb)
    , m_manager(manager) {
    m_next = tinytcp::get_current_ms() + m_ms;
}

Timer::Timer(uint64_t next)
    : m_next(next) {

}

bool Timer::cancel() {
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (m_cb) {
        m_cb = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    // 不能直接重置时间，因为是放在set中管理的，里面是按照m_next排序的，直接更改会影响set的结构
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    m_next = tinytcp::get_current_ms() + m_ms;
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now) {
    if (ms == m_ms && !from_now) {
        return true;
    }
    TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
    if (!m_cb) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if (from_now) {
        start = tinytcp::get_current_ms();
    }
    else {
        start = m_next - m_ms;
    }
    m_ms = ms;
    m_next = start + m_ms;
    m_manager->add_timer(shared_from_this(), lock);
    return true;
}


TimerManager::TimerManager() {
    m_previous_time = tinytcp::get_current_ms();
}

TimerManager::~TimerManager() {

}

Timer::ptr TimerManager::add_timer(uint64_t ms, std::function<net_err_t()> cb, bool recurring) {
    Timer::ptr timer(new Timer(ms, cb, recurring, this));
    RWMutexType::WriteLock lock(m_mutex);
    add_timer(timer, lock);
    return timer;
}

static net_err_t on_timer(std::weak_ptr<void> weak_cond, std::function<net_err_t()> cb) {
    // 检查weak_ptr指向的资源有没有被释放掉, 没有的情况下才执行回调函数
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        cb();
    }
    return net_err_t::NET_ERR_OK;
}

Timer::ptr TimerManager::add_condition_timer(uint64_t ms, std::function<net_err_t()> cb,
                                std::weak_ptr<void> weak_cond,
                                bool recurring) {
    return add_timer(ms, std::bind(&on_timer, weak_cond, cb), recurring);
}

uint64_t TimerManager::get_next_time() {
    RWMutexType::ReadLock lock(m_mutex);
    m_tickled = false;
    if (m_timers.empty()) {
        return ~0ULL; // 返回一个极大值
    }
    const Timer::ptr& next = *m_timers.begin();
    uint64_t now_ms = tinytcp::get_current_ms();
    if (now_ms >= next->m_next) {
        return 0;
    }
    return next->m_next - now_ms;
}


void TimerManager::list_expired_cb(std::vector<std::function<net_err_t()> >& cbs) {
    uint64_t now_ms = tinytcp::get_current_ms();
    std::vector<Timer::ptr> expired;
    {
        RWMutexType::ReadLock lock(m_mutex);
        if (m_timers.empty()) {
            return ;
        }
    }
    RWMutexType::WriteLock lock(m_mutex);
    if (m_timers.empty()) {
        return ;
    }

    bool roll_over = detect_clock_roll_over(now_ms);
    if (!roll_over && ((*m_timers.begin())->m_next > now_ms)) {
        return ;
    }

    Timer::ptr now_timer(new Timer(now_ms));
    auto it = roll_over ? m_timers.end() : m_timers.lower_bound(now_timer);
    while (it != m_timers.end() && (*it)->m_next == now_ms) {
        ++it;
    }
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    cbs.reserve(expired.size());
    for (auto& timer : expired) {
        cbs.push_back(timer->m_cb);
        if (timer->m_recurring) {
            timer->m_next = now_ms + timer->m_ms;
            m_timers.insert(timer);
        }
        else {
            timer->m_cb = nullptr;
        }
    }
}

void TimerManager::add_timer(Timer::ptr val, RWMutexType::WriteLock& lock) {
    auto it = m_timers.insert(val).first;
    bool at_front = (it == m_timers.begin() && !m_tickled);
    if (at_front) {
        m_tickled = true;
    }
    lock.unlock();
    if (at_front) {
        on_timer_inserted_at_front();
    }   
}

bool TimerManager::detect_clock_roll_over(uint64_t now_ms) {
    bool roll_over = false;
    if (now_ms < m_previous_time && 
        now_ms < (m_previous_time - 60 * 60 * 1000)) {
        roll_over = true;
    }
    m_previous_time = now_ms;
    return roll_over;
}

bool TimerManager::has_timer() {
    RWMutexType::ReadLock lock(m_mutex);
    return !m_timers.empty();
}




} // namespace tinytcp


