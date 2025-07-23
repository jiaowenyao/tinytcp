

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <thread>
#include <vector>
#include "src/lock_free_ring_queue.h"


using namespace tinytcp;


class LockFreeRingQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化队列大小
        m_queue_size = 64;  // 队列大小设置为4
        m_queue = std::make_unique<LockFreeRingQueue<int>>(m_queue_size);
    }

    std::unique_ptr<LockFreeRingQueue<int>> m_queue;
    uint32_t m_queue_size;
};

// 测试队列初始化
TEST_F(LockFreeRingQueueTest, Initialization) {
    EXPECT_EQ(m_queue->size(), 0U);
    EXPECT_TRUE(m_queue->is_empty());
}

// 测试入队和出队单个元素
TEST_F(LockFreeRingQueueTest, SingleEnqueueDequeue) {
    int value_in = 42;
    int value_out = 0;

    EXPECT_TRUE(m_queue->enqueue(value_in));
    EXPECT_EQ(m_queue->size(), 1U);
    EXPECT_FALSE(m_queue->is_empty());

    EXPECT_TRUE(m_queue->dequeue(&value_out));
    EXPECT_EQ(value_out, value_in);
    EXPECT_EQ(m_queue->size(), 0U);
    EXPECT_TRUE(m_queue->is_empty());
}

// 测试队列满时入队
TEST_F(LockFreeRingQueueTest, EnqueueFullQueue) {
    for (uint32_t i = 0; i < m_queue_size - 1; ++i) {  // 注意减1
        EXPECT_TRUE(m_queue->enqueue(static_cast<int>(i)));
    }

    EXPECT_EQ(m_queue->size(), m_queue_size - 1);
    EXPECT_FALSE(m_queue->enqueue(100));  // 队列已满，入队失败
}

// 测试空队列出队
TEST_F(LockFreeRingQueueTest, DequeueEmptyQueue) {
    int value_out = 0;
    EXPECT_FALSE(m_queue->dequeue(&value_out));  // 队列为空，出队失败
}

// 多线程测试
TEST_F(LockFreeRingQueueTest, MultiThreadedEnqueueDequeue) {
    const int num_threads = 4;
    const int num_elements_per_thread = 10;

    auto enqueue_function = [&](int thread_id) {
        for (int i = 0; i < num_elements_per_thread; ++i) {
            m_queue->enqueue(thread_id * num_elements_per_thread + i);
        }
    };

    auto dequeue_function = [&](int thread_id, int* result_array) {
        for (int i = 0; i < num_elements_per_thread; ++i) {
        int value = 0;
        while (!m_queue->dequeue(&value)) {
            std::this_thread::yield();
        }
        result_array[thread_id * num_elements_per_thread + i] = value;
        }
    };

    std::vector<std::thread> threads;
    int results[num_threads * num_elements_per_thread] = {0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(enqueue_function, i);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    threads.clear();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(dequeue_function, i, results);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(m_queue->size(), 0U);
    EXPECT_TRUE(m_queue->is_empty());

    // 检查所有元素是否都已被成功出队
    std::sort(std::begin(results), std::end(results));
    for (int i = 0; i < num_threads * num_elements_per_thread; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

// 边界条件测试：初始化大小为1的队列
TEST(LockFreeRingQueueBoundaryTest, InitializationWithSizeOne) {
    LockFreeRingQueue<int> small_queue(1);

    EXPECT_EQ(small_queue.size(), 0U);
    EXPECT_TRUE(small_queue.is_empty());

    int value_in = 99;
    EXPECT_TRUE(small_queue.enqueue(value_in));
    EXPECT_FALSE(small_queue.enqueue(value_in));  // 队列应该已经满了
}

// 边界条件测试：入队和出队仅一个元素
TEST(LockFreeRingQueueBoundaryTest, SingleElementQueue) {
    LockFreeRingQueue<int> small_queue(1);

    int value_in = 123;
    int value_out = 0;

    EXPECT_TRUE(small_queue.enqueue(value_in));
    EXPECT_FALSE(small_queue.enqueue(value_in));  // 队列已满

    EXPECT_TRUE(small_queue.dequeue(&value_out));
    EXPECT_EQ(value_out, value_in);

    EXPECT_FALSE(small_queue.dequeue(&value_out));  // 队列为空
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

