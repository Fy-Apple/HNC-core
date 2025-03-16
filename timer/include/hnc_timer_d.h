#pragma once

#include <functional>
#include <chrono>

namespace hnc::core::timer::details {

/**
 * @brief `timerfd` 定时器封装
 */
class HncTimer {
public:
    /**
     * @brief 系统调用timer fd 创建并设置超时时间
     */
    HncTimer(std::chrono::seconds duration, std::function<void()> &&callback, bool repeat);
    ~HncTimer();

    /**
     * @brief 用于返回创建的timer_fd 以加入epoll
     */
    [[nodiscard("timer fd !")]] int get_fd() const noexcept;

    /**
     * @brief 该定时器对应的回调事件
     */
    void callback() noexcept;

    // ..
    bool is_repeating() const noexcept;

private:
    int m_timer_fd_;
    bool m_is_repeat_;
    std::function<void()> m_callback_;
};

}  // namespace hnc::core::timer::details
