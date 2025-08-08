#include "memblock.h"


namespace tinytcp {


MemBlock::MemBlock(int block_size, int capacity)
    : m_block_size(block_size) {

    m_queue.reset(new LockFreeRingQueue<uint8_t*>(capacity));
    capacity = m_queue->capacity();

    m_block.reset(new uint8_t[block_size * capacity]);
    uint8_t *block_ptr = m_block.get();

    for (int i = 0; i < capacity; ++i, block_ptr += block_size) {
        m_queue->push(block_ptr);
    }
}

MemBlock::~MemBlock() {

}

bool MemBlock::alloc(void* ptr, int timeout_ms) {
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    uint8_t* _ptr = nullptr;
    return m_queue->pop(&_ptr);
}

bool MemBlock::free(const void* ptr) {
    return m_queue->push((uint8_t*)ptr);
}


} // namespace tinytcp



