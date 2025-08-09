#pragma once

/**
* memory block，用无锁队列封装一个简单的内存池
*/


#include <memory>
#include "src/lock_free_ring_queue.h"

namespace tinytcp {

class MemBlock {
public:
    using uptr = std::unique_ptr<MemBlock>;
    using  ptr = std::shared_ptr<MemBlock>;

    MemBlock(int block_size, int capacity = 1024);

    ~MemBlock();

    int block_size() const noexcept { return m_block_size; }

    int size() const noexcept { return m_queue->size(); }

    int capacity() const noexcept { return m_queue->capacity(); }

    /** TODO
    * ms < 0  : 无限等待
    * ms = 0  : try pop
    * ms > 0  : 加上超时处理
    */
    bool alloc(void** ptr, int timeout_ms);

    bool free(const void* ptr);

private:
    int m_block_size;                  // 每个内存块的大小
    std::unique_ptr<uint8_t> m_block;  // 内存块的首地址, 分配的内存块
    std::unique_ptr<LockFreeRingQueue<uint8_t *>> m_queue;
};


} // namespace tinytcp


