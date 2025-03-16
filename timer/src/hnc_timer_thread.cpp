#include "hnc_timer_thread.h"

#include <hnc_timer_manager.h>
#include <iostream>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <string.h>
#include <vector>

namespace hnc::core::timer::details {


HncTimerThread::HncTimerThread() : m_manager_(nullptr), m_epoll_fd_(-1 ),m_event_fd_(-1),  m_running_(false) { }

/**
 * @brief 初始化后台线程， 创建epoll_fd 和 event_fd, 并获初始化线程池
 */
void HncTimerThread::init(const uint8_t threads, HncTimerManager* manager) noexcept {
    // 创建epoll
    m_epoll_fd_ = epoll_create1(0);
    if (m_epoll_fd_ == -1) {
        std::cerr << "Timer thread failed to create epoll_fd! \n";
        exit(EXIT_FAILURE);
    }
    // 创建event 用于唤醒后台线程退出
    m_event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_event_fd_ == -1) {
        std::cerr << "Timer thread failed to create event_fd! \n";
        exit(EXIT_FAILURE);
    }

    // 将event_fd添加到 epoll 树中
    m_add_fd_to_epoll(m_event_fd_);

    m_manager_ = manager;

    // 初始化线程池, 默认使用 定长线程池
    m_thread_pool_ = thread_pool::ThreadPoolManager::get_fixed_pool("TimerThreadPool", threads);
    // 启动线程池
    m_thread_pool_->start();
    m_running_ = true; // 这里不需要内存序， 因为子线程还没启动
    // 使用其中一个线程 作为定时器监听线程,  必须重载operator() 才能加入线程池
    m_thread_pool_->submit_task([this]()->void { this->operator()(); });

}


HncTimerThread::~HncTimerThread() {
    // stop由manager 显式调用过了， 因为是manager的析构先调用， 随后才会调用本HncTimerThread的析构
    // stop()
}

/**
 * @brief 将timer_fd 加入epoll
 */
void HncTimerThread::add_timer(const int timer_fd) const noexcept {
    m_add_fd_to_epoll(timer_fd);
}

void HncTimerThread::remove_timer(const int timer_fd) const noexcept{
    // if (epoll_ctl(m_epoll_fd_, EPOLL_CTL_DEL, timer_fd, nullptr) == -1) {
    //     std::cerr << "Timer thread delete fd " << timer_fd << " from epoll failed: " << strerror(errno) << std::endl;
    // }
    epoll_ctl(m_epoll_fd_, EPOLL_CTL_DEL, timer_fd, nullptr);
    ::close(timer_fd);
}

/**
     * @brief 将停止后台线程
     */
void HncTimerThread::stop() noexcept {
    // 通知线程池中的 定时器线程 停止执行
    constexpr uint64_t signal = 1;
    // 先改结果
    m_running_.store(false, std::memory_order_release);
    // 再通知
    write(m_event_fd_, &signal, sizeof(signal));

    // TODO : 是否需要等待 定时器线程退出再析构

    lch.wait();
    logger::log_debug("HncTimerManager stop! timer thread is exiting! ......");


    // 关闭epoll和event
    close(m_epoll_fd_);
    close(m_event_fd_);
}


void HncTimerThread::operator()() const noexcept {
    std::vector<epoll_event> events(8);
    uint64_t signal{};
    while (m_running_.load(std::memory_order_acquire)) {
        const int event_num = epoll_wait(m_epoll_fd_, events.data(), 10, -1);
        for (int i = 0; i < event_num; ++i) {
            int fd = events[i].data.fd;
            if (fd == m_event_fd_) {
                // event fd， 说明是主线程要退出了， 通知后台定时器线程正常退出
                read(m_event_fd_, &signal, sizeof(signal));
                // 通知主线程退出
                lch.count_down();
                std::cout << "Timer thread exit !\n";
                return;
            }
            // 否则一定是定时器 时间到了， 提交一个任务到线程池执行, read 由对应的回调内会执行
            m_thread_pool_->submit_task([this, fd]() {m_manager_->callback(fd);});
        }
    }
}

/**
 * @brief 添加 timer fd 或 event fd 进epoll
 */
void HncTimerThread::m_add_fd_to_epoll(const int fd) const noexcept {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET; // 监听可读事件，使用边缘触发模式
    ev.data.fd = fd;

    // epoll_ctl 是 线程安全的 ， 不需要任何加锁
    if (epoll_ctl(m_epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
        // 添加失败不需要退出程序，打印一下错误日志
        std::cerr << "Timer thread add fd " << fd << " to epoll failed: " << strerror(errno) << std::endl;
        ::close(fd); // 加入epoll失败，关闭fd避免泄漏
    }
    std::cout << "Timer thread add fd " << fd << " to epoll success\n";
}

}  // namespace hnc::core::timer::details
