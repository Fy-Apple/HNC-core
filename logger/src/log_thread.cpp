#include "log_thread.h"

#include "log_common.h"

#include "log_buffer.h"

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <iostream>
#include <filesystem>
#include <latch>


namespace hnc::core::logger::details {

LogThread::LogThread()
    : m_running_(false)
    , m_primary_buffer_(&buffers[0])
    , m_secondary_buffer_(&buffers[1])
    , m_write_buffer_(&buffers[2]) {

    // 若日志文件目录不存在， 优先创建目录
    std::filesystem::create_directories(std::filesystem::path(constant::LOG_FILE_NAME).parent_path());

    m_logfile_.open(constant::LOG_FILE_NAME);

    // 先初始化所有的fd
    m_init_fd();
    if (!m_logfile_.is_open()) {
        std::cerr << "无法打开日志文件: " + constant::LOG_FILE_NAME + '\n';
        exit(EXIT_FAILURE);
    }
}

LogThread::~LogThread() {
    m_running_.store(false, std::memory_order_release); // release内存序保证其他线程可以看到改变
    // 发送 event fd 通知，唤醒日志线程，使其退出
    notify();
    // 等待日志线程退出
    if (m_log_thread_.joinable()) {
        // 等待子线程把当前所有日志都写完后再退出
        std::cout << "[主线程] wait sub thread join...\n";
        m_log_thread_.join();
    }

    // 关闭输出文件流
    if (m_logfile_.is_open()) {
        m_logfile_.close();
    }

    // 关闭文件描述符
    close(m_event_fd_);
    close(m_epoll_fd_);
    std::cout << "[主线程] exit...\n";
}


void LogThread::start() {
    // 原子交换 exchange 返回旧值
    if (m_running_.exchange(true)) {
        return;  // 日志线程已在运行
    }


    // 启动日志线程
    m_log_thread_ = std::thread([this]() { this->m_log_thread_func(); });

    // 等待子线程真正启动
    m_latch_.wait();

    // 等待线程真正启动
    while (!m_running_.load()) {
        std::this_thread::yield();  // 让出 CPU，等待 `m_log_thread_func()` 启动
    }
}


/**
 * @brief  通过写入event fd 通知后台日志线程苏醒，切换缓冲区 ， 并将日志 write 到日志文件
 */
void LogThread::notify() const noexcept {
    constexpr uint64_t val = 1; // 发送通知
    std::cout << "[主线程] notify...\n";
    write(m_event_fd_, &val, sizeof(val));
}

/**
 * @brief 线程屏障， 用于等待日志线程交换完从缓冲区和备份缓冲区
 */
void LogThread::wait() const noexcept {
    m_barrier_.wait(std::move(m_barrier_.arrive()));
}

/**
 * @brief 提供给生产者线程使用的接口， 用于切换 主从缓冲区
 */
void LogThread::primary_swap() const noexcept {
    std::swap(m_primary_buffer_, m_secondary_buffer_);
}


void LogThread::m_init_fd() noexcept{
    // 创建 event fd，初始值为 0
    m_event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 设置efd 为 非阻塞， 并且 fork出的子进程不会继承该文件描述符
    // 还有一个参数为 EFD_SEMAPHORE  表示可以被多次read， 每次只会 - 1; 此处暂时不需要
    if (m_event_fd_ == -1) {
        std::cerr << "event fd 创建失败\n";
        exit(EXIT_FAILURE);
    }

    // 这里可以不用epoll, 直接用read阻塞也可以, 后续可以增加 epoll监听的事件
    m_epoll_fd_ = epoll_create1(0); // epoll_create 的 flag 不能为 0
    if (m_epoll_fd_ == -1) {
        std::cerr << "epoll fd 创建失败\n";
        exit(EXIT_FAILURE);
    }

    // 设置epoll监听该event_fd的唤醒事件
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = m_event_fd_;
    // ev.data.ptr = ... if 有需要
    if (epoll_ctl(m_epoll_fd_, EPOLL_CTL_ADD, m_event_fd_, &ev) == -1) {
        perror("epoll 监听 event fd 读事件 失败\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief 后台日志线程的主函数，监听 `event fd` 事件，触发后 交换 从备缓冲区， 并写入 OS page cache
 */
void LogThread::m_log_thread_func() noexcept{
    // 内核会将 发生事件的描述符拷贝到该数组缓冲区中
    epoll_event events[1];
    // event fd 读出来的计数器
    uint64_t val;
    // 通知主线程可以再次启动了
    m_latch_.count_down();
    while (m_running_.load()) {
        std::cout << "[子线程] epoll_wait...\n";
        if (epoll_wait(m_epoll_fd_, events, 1, -1) == -1 && errno != EINTR) {
            if (errno == EINTR) {
                // EINTR 表示 错误是被信号中断的, 重新调用epoll监听， lt模式
                continue;
            }
            std::cerr << "log thread -> epoll wait 失败\n";
            break;
        }
        // 先检查是否是需要退出线程
        if (!m_running_.load(std::memory_order_acquire)) {
            break;
        }
        // 这里一定是 event fd的可读事件, 直接read即可
        //if (events[0].data.fd == event_fd) {
        // 读取 event fd（清空计数器）
        read(m_event_fd_, &val, sizeof(val));

        // 主从切换和 从备切换不需要加锁， 因为可以保证 一定是 生产者切换完成后才通知消费者
        std::swap(m_secondary_buffer_, m_write_buffer_);
        // 交换完 缓冲区 后, 到达线程屏障，使生产者线程醒来
        // TODO : NOT TODO , 神坑 arrive_and_drop 是 永久减少一个线程到达,
        m_barrier_.arrive_and_wait();

        // 将write缓冲区的日志写入 日志 文件
        m_write_buffer_->flush(m_logfile_, true);
    }
    std::cout << "[子线程] ready exit ! write back log...\n";
    // 推出前将可能存在的日志再写入文件, 按照先后顺序写入日志
    m_write_buffer_->flush(m_logfile_, false);
    m_secondary_buffer_->flush(m_logfile_, false);
    m_primary_buffer_->flush(m_logfile_, false);
    std::cout << "[子线程] exit...\n";
}

}











