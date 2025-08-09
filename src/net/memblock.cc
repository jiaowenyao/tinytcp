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

bool MemBlock::alloc(void** ptr, int timeout_ms) {
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    bool ok = m_queue->pop((uint8_t**)ptr);
    return ok;
}

bool MemBlock::free(const void* ptr) {
    bool ok = m_queue->push((uint8_t*)ptr);
    return ok;
}


} // namespace tinytcp



