#include "hnc_thread_pool.h"

#include <hnc_thread.h>

#include <ranges>

namespace hnc::core::thread_pool::details {

HncThreadPool::HncThreadPool(const TPoolMode mode, const uint8_t init_thread_size)
    : m_init_size_(init_thread_size)
    , m_cur_size_(init_thread_size)
    , m_thresh_hold_thread_size_(constant::THREAD_HOLD_THREAD_SIZE)
    , m_idle_size_(0)
    , m_task_size_(0)
    , m_thresh_hold_task_size_(constant::THRESH_HOLD_TASK_SIZE)
    , m_mode_(mode)
    , m_running_(false){

}

HncThreadPool::~HncThreadPool() {
    m_running_.store(false, std::memory_order_release);

    // 析构时要等待所有线程都析构在退出
    std::unique_lock<std::mutex> locker(m_task_mtx_);
    m_cond_not_empty_.notify_all();

    m_cond_exit_.wait(locker, [&]() -> bool { return m_threads_.empty();});
}

/**
 * @brief 启动线程池
 */
void HncThreadPool::start() noexcept {
    for (size_t i = 0; i < m_init_size_; ++i) {
        auto cur_thread = std::make_unique<HncThread>([this](const int thread_id) -> void {
            if (this->is_fixed()) this->m_fixed_func(thread_id);
            else this->m_cached_func(thread_id);
        });
        auto cur_tid = cur_thread->get_thread_id();
        m_threads_.emplace(cur_tid, std::move(cur_thread));
    }
    // 启动线程
    m_running_.store( true, std::memory_order_release);

    // cpp20 的ranges库 可以 更方便的遍历 map容器的value 了
    for (const auto& value : m_threads_ | std::views::values) {
        // 启动线程池中所有线程
        value->start();
    }

    // 初始空闲线程数
    m_idle_size_ = m_init_size_;
}

/**
 * @brief 判断是否为固定线程数线程池
 */
bool HncThreadPool::is_fixed() const noexcept {
    return m_mode_ == TPoolMode::FIXED;
}

/**
 * @brief 打印输出线程池状态
 */
void HncThreadPool::print_status() const noexcept {
    if (is_fixed()) {
        std::cout << " init_thead : " << m_init_size_
        << "\n task_size : " << m_task_size_ << '/' << m_thresh_hold_task_size_ << '\n';
    } else {
        std::cout << " init_thead : " << m_init_size_ << '/' << m_thresh_hold_thread_size_
           << "\n cur_thread : " << m_cur_size_ << '/' << m_thresh_hold_thread_size_
           << "\n idle_thread : " << m_idle_size_ << '/' << m_thresh_hold_thread_size_
           << "\n task_size : " << m_task_size_ << '/' << m_thresh_hold_task_size_ << '\n';
    }
}

/**
 * @brief 调整线程池中任务数量上限,
 * @param task_thresh_hold 任务上限阈值
 * @return 线程池已启动则返回false
 */
bool HncThreadPool::set_task_thresh_hold(const size_t task_thresh_hold) noexcept {
    if (m_check_running()) return false;
    m_thresh_hold_task_size_ = task_thresh_hold;
    return true;
}


/**
 * @brief 提供给线程运行 的  固定数量线程函数
 */
void HncThreadPool::m_fixed_func(const int tid) noexcept {
    const std::string id_str = std::to_string(tid);
    while (true) {
        std::function<void()> task;
        {
            // cpp17 推出的 模板类型推导，可以根据参数确定模板类型，所以不写<std::mutex> 也可以
            std::unique_lock<std::mutex> locker(m_task_mtx_);
            logger::log_debug("[fixed]thread" + id_str + "-> try get task...");
            while (m_task_size_.load(std::memory_order_acquire) == 0) {
                // 唤醒后查看是否需要退出线程池
                if (!m_check_running()) {
                    // 不需要加锁，因为这里持有task锁，确保只有一个线程再操作哈希表
                    m_threads_.erase(tid);
                    // 若是最后一个线程退出则通知线程池持有线程退出
                    if (m_threads_.empty()) m_cond_exit_.notify_one();
                    return ;
                }
                // 等待 任务队列加入新的task
                m_cond_not_empty_.wait(locker);
            }
            logger::log_debug("[fixed]thread" + id_str + "-> get task");
            m_get_task(task);
        }
        // 执行任务 fixed模式下不需要修改 idle size， 只会给可变模式下使用
        task();
    }
}

/**
 * @brief 提供给线程运行的 可变线程数函数
 */
void HncThreadPool::m_cached_func(const int tid) noexcept {
    const std::string id_str = std::to_string(tid);
    auto last = std::chrono::high_resolution_clock::now();
    while (true) {
        std::function<void()> task;
        {
            // cpp17 推出的 模板类型推导，可以根据参数确定模板类型，所以不写<std::mutex> 也可以
            std::unique_lock<std::mutex> locker(m_task_mtx_);
            logger::log_debug("[cached]thread" + id_str + "-> try get task...");

            // TODO:  m_task_size 在mutex的保护下，可以改成 普通变量, 减少atomic的开销
            while (m_task_size_.load(std::memory_order_acquire) == 0) {
                // 唤醒后查看是否需要退出线程池
                if (!m_check_running()) {
                    // 不需要加锁，因为这里持有task锁，确保只有一个线程再操作哈希表
                    m_threads_.erase(tid);
                    // 若是最后一个线程退出则通知线程池持有线程退出
                    if (m_threads_.empty()) m_cond_exit_.notify_one();
                    return ;
                }

                // 可变线程模式下， 当等待任务超时时，尝试回收线程
                if (std::cv_status::timeout == m_cond_not_empty_.wait_for(locker, std::chrono::seconds(1))) {
                    auto now = std::chrono::high_resolution_clock::now();
                    // 超时等待 并且 当前线程池数量 > 初始线程数 -->> 回收多余线程数
                    if (auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last); dur.count() >= constant::THREAD_IDLE_TIME && m_cur_size_ > m_init_size_) {
                        // 开始回收当前线程
                        m_threads_.erase(tid);
                        m_cur_size_.fetch_sub(1, std::memory_order_release);
                        m_idle_size_.fetch_sub(1, std::memory_order_release);
                        logger::log_debug("[cached]thread" + id_str + "-> timeout...exit!");
                        return;
                    }
                }
            }
            logger::log_debug("[cached]thread" + id_str + "-> get task");
            m_get_task(task);
        }
        // 执行任务
        m_idle_size_.fetch_sub(1, std::memory_order_release);
        task();
        m_idle_size_.fetch_add(1, std::memory_order_release);
        // cache模式下 更新 执行时间
        last = std::chrono::high_resolution_clock::now();
    }
}

/**
 * @brief 判断是否需要从线程池中退出，再函数外部加锁， 从哈希表中删除线程并退出
 */
bool HncThreadPool::m_is_exit(const int tid) noexcept
{
    if (!m_check_running()) {
        // 不需要加锁，因为这里持有task锁，确保只有一个线程再操作哈希表
        m_threads_.erase(tid);
        // 若是最后一个线程退出则通知线程池持有线程退出
        if (m_threads_.empty()) m_cond_exit_.notify_one();
        return true;
    }
    return false;
}

/**
 * @brief 获取一个任务 并尝试唤醒其他线程继续获取任务
 */
void HncThreadPool::m_get_task(std::function<void()>& task) noexcept
{
    task = m_task_que_.front();
    m_task_que_.pop();
    if (m_task_size_.fetch_sub(1, std::memory_order_release)) {
        // 任务队列还有别的任务 唤醒其他消费者
        m_cond_not_empty_.notify_all();
    }
}


}
