#pragma once
#include <vector>
#include <functional>
#include <atomic>
#include <optional>
#include <iostream>


class LockFreeRingQueue {
public:
    explicit LockFreeRingQueue(size_t size) : size_(size), mask_(size - 1), buffer_(size), head_(0), tail_(0) {
        if ((size & (size - 1)) != 0) {
            throw std::runtime_error("Queue size must be power of 2");
        }
    }

    // 禁止拷贝
    LockFreeRingQueue(const LockFreeRingQueue&) = delete;
    LockFreeRingQueue& operator=(const LockFreeRingQueue&) = delete;

    // 插入任务（无锁）
    bool push(std::function<void()> task) {
        uint32_t tail = tail_.load(std::memory_order_relaxed);
        uint32_t next_tail = (tail + 1) & mask_;

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // 队列满
        }

        buffer_[tail] = std::move(task);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    // 获取任务（无锁）
    std::optional<std::function<void()>> pop() {
        uint32_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // 队列空
        }

        std::function<void()> task = std::move(buffer_[head]);
        head_.store((head + 1) & mask_, std::memory_order_release);
        return task;
    }

    // 判断队列是否为空
    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    // 判断队列是否已满
    bool full() const {
        return ((tail_.load(std::memory_order_acquire) + 1) & mask_) == head_.load(std::memory_order_acquire);
    }

private:
    const size_t size_;              // 队列大小
    const uint32_t mask_;            // 用于计算索引 `(index & mask_)`
    std::vector<std::function<void()>> buffer_;  // 任务存储

    alignas(64) std::atomic<uint32_t> head_; // 头指针
    alignas(64) std::atomic<uint32_t> tail_; // 尾指针
};
