#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>
#include "hnc_timer_thread.h"

namespace hnc::core::timer::details {

class HncTimer;

/**
 * @brief 管理多个 `timerfd`
 */
class HncTimerManager {
public:
    /**
     * @brief 创建一个定时器管理者类， 需要指定后台定时器线程数量
     */
    explicit HncTimerManager();

    /**
     * @brief  TODO: 需要等待后台定时器线程退出吗 ?
     */
    ~HncTimerManager();

    /**
     * @brief 由于本实例再 构造函数时未初始化this， 需要把this指针传给 timer thread， 所以需要额外辅助函数而不能再构造方法中初始化
     */
    void start(uint8_t threads) noexcept;

    /**
     * @brief 生产者线程提交新的定时任务，加入epoll中
     */
    int add_timer(std::chrono::seconds duration, std::function<void()> callback, bool repeat = false);

    /**
     * @brief 从epoll中删除一个定时器任务
     */
    void remove_timer(int timer_fd);

    /**
     * @brief 执行对应定时器的回调
     */
    void callback(int timer_fd) noexcept;

private:

    // 管理所有的定时器
    std::unordered_map<int, std::shared_ptr<HncTimer>> m_timers_;
    std::mutex m_mtx_;

    // 定时器后台 epoll 线程
    HncTimerThread m_timer_thread_;
};

}  // namespace hnc::core::timer::details
