#pragma once

#include <atomic>
#include <memory>

#include "hnc_thread_pool.h"

namespace hnc::core::timer::details {

class HncTimerManager;

/**
 * @brief 后台定时器线程类， 负责监听 所有定时器 fd, 发生可读事件后调用线程池去执行
 */
class HncTimerThread {
public:

    HncTimerThread();
    ~HncTimerThread();

    /**
     * @brief 初始化后台线程， 创建epoll_fd 和 event_fd, 初始化线程池， 启动后台定时器线程
     */
    void init(uint8_t threads, HncTimerManager* manager) noexcept ;

    /**
     * @brief 将timer_fd 加入epoll
     */
    void add_timer(int timer_fd) const noexcept;

    /**
     * @brief 将timer_fd 从epoll 中删除
     */
    void remove_timer(int timer_fd) const noexcept;

    /**
     * @brief 将停止后台线程
     */
    void stop() noexcept;

    /**
     * @brief 线程中的任务对象必须 重载operator 才能加入线程池中任务
     */
    void operator()() const noexcept;

private:
    /**
     * @brief 添加 timer fd 或 event fd 进epoll
     */
    void m_add_fd_to_epoll(int fd) const noexcept;

    // 用于执行定时器监听和回调的线程池
    std::shared_ptr<thread_pool::details::HncThreadPool> m_thread_pool_;

    HncTimerManager *m_manager_;

    // epoll 负责监听所有定时器
    int m_epoll_fd_;
    // event 负责告知后台线程正常退出
    int m_event_fd_;
    // 退出标志位
    std::atomic<bool> m_running_;
    // 用于主线程等待 定时器线程退出
    mutable std::latch lch{1};
};

}  // namespace hnc::core::timer::details
