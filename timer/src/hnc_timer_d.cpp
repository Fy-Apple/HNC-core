#include "hnc_timer_d.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include "hnc_log.h"

namespace hnc::core::timer::details {

/**
 * @brief 系统调用timer fd 创建并设置超时时间
 */
HncTimer::HncTimer(const std::chrono::seconds duration, std::function<void()> &&callback, const bool repeat)
    : m_timer_fd_(-1), m_is_repeat_(repeat), m_callback_(callback){

    // 创建一个 timer fd, 使用内核的CLOCK_MONOTONIC 单调时钟（不受系统时间影响），
    // CLOCK_REALTIME 系统实时时钟 受到系统时间调整的影响， 会影响定时器
    // TFD_NONBLOCK 非阻塞，TFD_CLOEXEC 描述符不继承给子进程， 后续交付给epoll监听
    m_timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (m_timer_fd_ == -1) {
        throw std::runtime_error("Failed to create timer fd");
    }

    struct itimerspec new_value{}; // 已经把interval初始化为0了
    new_value.it_value.tv_sec = duration.count();
    // TODO: 添加毫秒实现
    // new_value.it_value.tv_nsec = ...;
    if (repeat) {
        // 设置是否重复定时触发任务
        new_value.it_interval = new_value.it_value;
    }
    // 设置 定时器
    // timer fd, 0 -> 相对时间， TFD_TIMER_ABSTIME -> 绝对时间（一般不使用）
    // new_value : 指定新的超时时间, old_value : 传nullptr即可
    timerfd_settime(m_timer_fd_, 0, &new_value, nullptr);
    logger::log_debug("add new timer ! fd = " + std::to_string(m_timer_fd_));
}

HncTimer::~HncTimer() {
    close(m_timer_fd_);
}

/**
 * @brief 用于返回创建的timer_fd 以加入epoll
 */
int HncTimer::get_fd() const noexcept{
    return m_timer_fd_;
}

/**
 * @brief 该定时器对应的回调事件, 超时后由线程池去执行该回调
 */
void HncTimer::callback() noexcept{
    uint64_t expirations;
    // 读取该fd超时次数， 执行 对应次数 的回调任务
    read(m_timer_fd_, &expirations, sizeof(expirations));
    for (uint64_t i = 0; i < expirations; ++i) {
        m_callback_();
        logger::log_debug("exec callback ! fd = " + std::to_string(m_timer_fd_));
    }
}

bool HncTimer::is_repeating() const noexcept {
    return m_is_repeat_;
}

}  // namespace hnc::core::timer::details
