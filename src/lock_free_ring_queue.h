#pragma once


#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>

#include <memory>
#include <thread>

#include "noncopyable.h"

namespace tinytcp {

template <typename T>
class LockFreeRingQueue : Noncopyable {
public:
    explicit LockFreeRingQueue(uint32_t size);

    ~LockFreeRingQueue() = default;

    bool enqueue(const T& data);

    bool dequeue(T* data);

    bool is_empty() const noexcept;

    bool is_full() const noexcept;

    uint32_t size() const noexcept;

private:
    static bool is_power_of_two(uint32_t num) noexcept;

    static uint32_t ceil_power_of_two(uint32_t num) noexcept;

    static uint32_t round_uppower_of_two(uint32_t num) noexcept;

    uint32_t index_of_queue(uint32_t index) const noexcept;

private:
    const uint32_t m_size;                     // Size of the queue, must be a power of two
    std::atomic<uint32_t> m_length;            // Current length of the queue
    std::atomic<uint32_t> m_read_index;        // Index for the consumer to read
    std::atomic<uint32_t> m_write_index;       // Index for the producer to write
    std::atomic<uint32_t> m_last_write_index;  // Last confirmed write index
    std::unique_ptr<T[]>  m_queue;             // Array to store the queue elements
};

template <typename T>
LockFreeRingQueue<T>::LockFreeRingQueue(uint32_t size)
    : m_size(size <= 1U             ? 2U
            : is_power_of_two(size) ? size
                                    : round_uppower_of_two(size)),
      m_length(0U),
      m_read_index(0U),
      m_write_index(0U),
      m_last_write_index(0U),
      m_queue(std::make_unique<T[]>(m_size)) {
    if (size == 0U) {
        throw std::out_of_range("Queue size must be greater than 0");
    }
}

template <typename T>
bool LockFreeRingQueue<T>::enqueue(const T& data) {
    uint32_t current_read_index;
    uint32_t current_write_index;

    do {
        current_read_index  = m_read_index.load(std::memory_order_relaxed);
        current_write_index = m_write_index.load(std::memory_order_relaxed);

        // Check if the queue is full
        if (index_of_queue(current_write_index + 1U) == index_of_queue(current_read_index)) {
            return false;  // Queue is full
        }
    } while (!m_write_index.compare_exchange_weak(current_write_index, current_write_index + 1U, std::memory_order_release,
                                                std::memory_order_relaxed));

    m_queue[index_of_queue(current_write_index)] = data;

    // Confirm the write operation
    while (!m_last_write_index.compare_exchange_weak(current_write_index, current_write_index + 1U,
                                                   std::memory_order_release, std::memory_order_relaxed)) {
        std::this_thread::yield();  // Yield CPU to avoid busy-waiting
    }

    m_length.fetch_add(1U, std::memory_order_relaxed);

    return true;
}

template <typename T>
bool LockFreeRingQueue<T>::dequeue(T* data) {
    if (data == nullptr) {
        throw std::invalid_argument("Null pointer passed to Dequeue");
    }

    uint32_t current_read_index;
    uint32_t current_last_write_index;

    do {
        current_read_index = m_read_index.load(std::memory_order_relaxed);
        current_last_write_index = m_last_write_index.load(std::memory_order_relaxed);

        // Check if the queue is empty
        if (index_of_queue(current_last_write_index) == index_of_queue(current_read_index)) {
            return false;  // Queue is empty
        }

        *data = m_queue[index_of_queue(current_read_index)];

        if (m_read_index.compare_exchange_weak(current_read_index, current_read_index + 1U, std::memory_order_release,
                                               std::memory_order_relaxed)) {
            m_length.fetch_sub(1U, std::memory_order_relaxed);
            return true;
        }
    } while (true);
}

template <typename T>
bool LockFreeRingQueue<T>::is_empty() const noexcept {
    return m_length.load(std::memory_order_relaxed) == 0U;
}

template <typename T>
bool LockFreeRingQueue<T>::is_full() const noexcept {
    uint32_t next_write_index = index_of_queue(m_write_index.load(std::memory_order_relaxed) + 1U);
    return next_write_index == m_read_index.load(std::memory_order_acquire);
}

template <typename T>
uint32_t LockFreeRingQueue<T>::size() const noexcept {
    return m_length.load(std::memory_order_relaxed);
}

template <typename T>
bool LockFreeRingQueue<T>::is_power_of_two(uint32_t num) noexcept {
    return (num != 0U) && ((num & (num - 1U)) == 0U);
}

template <typename T>
uint32_t LockFreeRingQueue<T>::ceil_power_of_two(uint32_t num) noexcept {
    num |= (num >> 1U);
    num |= (num >> 2U);
    num |= (num >> 4U);
    num |= (num >> 8U);
    num |= (num >> 16U);
    return num - (num >> 1U);
}

template <typename T>
uint32_t LockFreeRingQueue<T>::round_uppower_of_two(uint32_t num) noexcept {
    return ceil_power_of_two((num - 1U) << 1U);
}

template <typename T>
uint32_t LockFreeRingQueue<T>::index_of_queue(uint32_t index) const noexcept {
    return index & (m_size - 1U);
}





};

