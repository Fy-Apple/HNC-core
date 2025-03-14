#pragma once
#include <atomic>
#include <thread>
#include <fstream>
#include <barrier>
#include <latch>
#include <functional>


namespace hnc::core::logger::details {
// 前向声明
class LogBuffer;

class LogThread {
public:
    LogThread();
    ~LogThread();

    LogThread(const LogThread&) = delete;
    LogThread(LogThread &&) = delete;

    LogThread& operator=(const LogThread&) = delete;
    LogThread& operator=(LogThread &&) = delete;


    void start();

    void stop();
    // 通知后台线程交换缓冲区并将日志写入文件
    void notify() const noexcept;

    /**
     * @brief 线程屏障， 用于等待日志线程交换完从缓冲区和备份缓冲区
     */
    void wait() const noexcept;

    /**
     * @brief 提供给生产者线程使用的接口， 用于切换 主从缓冲区
     */
    void primary_swap() const noexcept;

    // 提供给生产者线程使用的 主缓冲区接口
    LogBuffer * primary_buffer() const noexcept { return m_primary_buffer_; }

    // 提供给生产者线程使用的 从缓冲区接口
    LogBuffer * secondary_buffer() const noexcept { return m_secondary_buffer_; }


private:
    void m_init_fd() noexcept;

    /**
     * @brief 日志线程执行函数
     */
    void m_log_thread_func() noexcept;


    std::atomic<bool> m_running_;
    std::thread m_log_thread_;
    std::ofstream m_logfile_;

    // 备份缓冲区（消费者写入文件）
    mutable LogBuffer *m_primary_buffer_;   // 主缓冲区（生产者写）
    mutable LogBuffer *m_secondary_buffer_; // 从缓冲区（交换用）
    LogBuffer *m_write_buffer_; // 备份缓冲区, 写满的缓冲区

    // 单次使用同步器， 用于主线程等待日志线程创建完毕
    std::latch m_latch_{1};

    // 线程屏障， 用于通知消费者线程  从备缓冲区切换完成
    mutable std::barrier<std::function<void()>> m_barrier_{1, [] {}};

    int m_event_fd_;  // 用于线程间通知
    int m_epoll_fd_;  // epoll 监听
};


}