#pragma once

#include "timer_manager.h"
#include <sys/epoll.h>
#include <thread>
#include <atomic>

/**
 * @brief 独立线程，监听 `timerfd` 触发事件
 */
class HncTimerThread {
public:
    explicit HncTimerThread(TimerManager& manager) : manager_(manager), running_(true) {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            throw std::runtime_error("Failed to create epoll");
        }
        worker_thread_ = std::thread([this]() { event_loop(); });
    }

    ~HncTimerThread() {
        running_ = false;
        close(epoll_fd_);
        if (worker_thread_.joinable()) worker_thread_.join();
    }

    void add_timer(int timer_fd) {
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = timer_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd, &event);
    }

    void remove_timer(int timer_fd) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, timer_fd, nullptr);
    }

private:
    void event_loop() {
        epoll_event events[10];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, 10, 1000);
            for (int i = 0; i < n; ++i) {
                int timer_fd = events[i].data.fd;
                HncTimer* timer = manager_.get_timer(timer_fd);
                if (timer) {
                    timer->execute_task();
                    if (!timer->is_repeating()) {
                        remove_timer(timer_fd);
                        manager_.remove_timer(timer_fd);
                    }
                }
            }
        }
    }

    int epoll_fd_;
    TimerManager& manager_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
};
