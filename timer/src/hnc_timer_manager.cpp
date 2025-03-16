#include "hnc_timer_manager.h"

#include <hnc_timer_d.h>
#include <unistd.h>

namespace hnc::core::timer::details {

/**
* @brief 创建一个定时器管理者类， 需要指定后台定时器线程数量
*/
HncTimerManager::HncTimerManager() {

}




HncTimerManager::~HncTimerManager() {
    // 需要等待 成员变量退出后才能析构，因为成员变量持有自己的指针
    // stop 方法会阻塞， 等待 后台定时器线程正常退出后采取析构自己， 随后析构 timer thread
    m_timer_thread_.stop();
}

/**
 * @brief 由于本实例再 构造函数时未初始化this， 需要把this指针传给 timer thread， 所以需要额外辅助函数而不能再构造方法中初始化
 */
void HncTimerManager::start(const uint8_t threads) noexcept {
    // 后台线程初始化
    // TODO : 加一个判断， 避免二次启动



    m_timer_thread_.init(threads, this);
    logger::log_debug("HncTimerManager start!");
}


/**
 * @brief 生产者线程提交新的定时任务
 */
int HncTimerManager::add_timer(const std::chrono::seconds duration, std::function<void()> callback, const bool repeat) {
    // 创建一个新的定时器， 内部会调用timer fd create
    auto timer_ptr = std::make_shared<HncTimer>(duration, std::move(callback), repeat);
    int timer_fd = timer_ptr->get_fd();

    // map是非线程安全的，所以这里要加锁，可能有多线程在操作
    std::lock_guard<std::mutex> lock(m_mtx_);
    m_timers_.emplace(timer_fd, timer_ptr);
    // 通知 后台线程 监听该定时器
    m_timer_thread_.add_timer(timer_fd);
    return timer_fd;
}

/**
 * @brief 从epoll中删除一个定时器任务
 */
void HncTimerManager::remove_timer(const int timer_fd) {

    std::lock_guard<std::mutex> lock(m_mtx_);
    m_timers_.erase(timer_fd);
    logger::log_debug("remove timer_fd = " + std::to_string(timer_fd));
    std::cout << "remove fd" << timer_fd << std::endl;
    // 通知 后台线程 删除该定时器, 不需要加锁，因为epoll_ctl 是线程安全的
    m_timer_thread_.remove_timer(timer_fd);
}

/**
 * @brief 执行对应定时器对应的回调
 */
void HncTimerManager::callback(const int timer_fd) noexcept{
    std::shared_ptr<HncTimer> timer_ptr;
    {
        std::lock_guard<std::mutex> lock(m_mtx_);
        timer_ptr = m_timers_[timer_fd];
    }
    if (timer_ptr) timer_ptr->callback();
}


}  // namespace hnc::core::timer::details
