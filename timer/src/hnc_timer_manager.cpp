#include "timer_task.h"
#include <iostream>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/timerfd.h>

class TimerManager {
public:
    TimerManager() : epoll_fd_(epoll_create1(0)), running_(true) {
        if (epoll_fd_ == -1) {
            throw std::runtime_error("Failed to create epoll");
        }
        worker_thread_ = std::thread([this]() { event_loop(); });
    }

    ~TimerManager() {
        running_ = false;
        close(epoll_fd_);
        if (worker_thread_.joinable()) worker_thread_.join();
    }

    int add_timer(std::chrono::milliseconds duration, std::function<void()> callback, bool repeat = false) {
        int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (timer_fd == -1) {
            throw std::runtime_error("Failed to create timerfd");
        }

        struct itimerspec new_value{};
        new_value.it_value.tv_sec = duration.count() / 1000;
        new_value.it_value.tv_nsec = (duration.count() % 1000) * 1000000;
        if (repeat) {
            new_value.it_interval = new_value.it_value; // 设置周期性定时器
        }
        timerfd_settime(timer_fd, 0, &new_value, nullptr);

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = timer_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd, &event);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace(timer_fd, HncTimerTask(std::move(callback), repeat));
        }

        return timer_fd;
    }

    void remove_timer(int timer_fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tasks_.count(timer_fd)) {
            close(timer_fd);
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, timer_fd, nullptr);
            tasks_.erase(timer_fd);
        }
    }

private:
    void event_loop() {
        epoll_event events[10];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, 10, 1000);
            for (int i = 0; i < n; ++i) {
                int timer_fd = events[i].data.fd;
                uint64_t expirations;
                read(timer_fd, &expirations, sizeof(expirations));  // 清除触发标记

                std::function<void()> task;
                bool repeat = false;

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (tasks_.contains(timer_fd)) {
                        task = [&] { tasks_[timer_fd].execute(); };
                        repeat = tasks_[timer_fd].is_repeating();
                    }
                }

                if (task) task();

                if (!repeat) {
                    remove_timer(timer_fd);
                }
            }
        }
    }

    int epoll_fd_;
    std::unordered_map<int, HncTimerTask> tasks_;
    std::mutex mutex_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
};
