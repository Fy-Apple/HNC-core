#pragma once

#include <memory>
#include <unordered_map>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <iostream>

#include "hnc_thread.h"
#include "tp_common.h"
#include "hnc_log.h"
#include "hnc_task.h"

namespace hnc::core::thread_pool::details {


class HncThreadPool {
public:
    explicit HncThreadPool(TPoolMode, uint8_t init_thread_size);
    ~HncThreadPool();

    /**
     * @brief 启动线程池
     *
     */
    void start() noexcept;

    // 接受一个函数对象和任意参数
    template <class Func, typename... Args>
    auto submit_task(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        using ResultType = decltype(func(args...));
        auto task_ptr = std::make_shared<std::packaged_task<ResultType()>>(
          std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<ResultType> result = task_ptr->get_future();

        // RAII
        std::unique_lock<std::mutex> locker(m_task_mtx_);

        // 最多等待1秒，否则任务提交失败
        if (!m_cond_not_full_.wait_for(locker, std::chrono::seconds(1), [&]() -> bool { return m_task_size_.load(std::memory_order_acquire) < m_thresh_hold_task_size_; })) {
            locker.unlock();
            std::cerr << "task queue is full, submit task fail\n";
            logger::log_debug("task queue is full, submit task fail");
            auto emptyTask = std::make_shared<std::packaged_task<ResultType()>>(
              []() -> ResultType { return ResultType(); });
            // 返回给用户一个默认值 表示 任务提交失败
            return emptyTask->get_future();
        }
        // 任务队列有空余位置，加入任务
        // 类型擦除成了 std::function<void()>
        m_task_que_.emplace([task_ptr] { (*task_ptr)(); });
        m_task_size_.fetch_add(1, std::memory_order_release);

        // 通知有新的任务了，使用条件变量通知等待在empty上的消费者线程（线程池的线程）
        m_cond_not_empty_.notify_one();

        // cached模式：任务处理紧急，并且小而且快的任务，根据任务和线程数动态调整线程的数量
        if (m_mode_ == TPoolMode::CACHED && m_task_size_ > m_idle_size_ && m_cur_size_ < m_thresh_hold_task_size_)
        {
            std::cout << "createThreadId[" << std::this_thread::get_id() << "]\n";
            // 添加新线程
            auto cur_thread = std::make_unique<HncThread>([this](const int thread_id) -> void {
                // 这里的线程一定是可变模式，所以直接执行cached_func即可
                this->m_cached_func(thread_id);
            });
            auto cur_tid = cur_thread->get_thread_id();
            m_threads_.emplace(cur_tid, std::move(cur_thread));

            // 启动新线程
            m_threads_[cur_tid]->start();
            m_cur_size_.fetch_add(1, std::memory_order_release); // 当前线程总数 + 1
            m_idle_size_.fetch_add(1, std::memory_order_release);// 空闲线程数 + 1
        }

        return result; // std::future
    }

    /**
     * @brief 提交一个任务类对象
     * @param task 任务对象，并实现 operator()
     * @return std::future<T> 任务返回值
     */
    template <typename T, typename = std::enable_if_t<std::is_invocable_v<T>>>
    auto submit_task(T &&task) -> std::future<decltype(task())> {
        // 任务打包成 packaged_task
        using ResultType = decltype(task());
        auto task_ptr = std::make_shared<std::packaged_task<ResultType()>>(std::move(task));
        std::future<ResultType> result = task_ptr->get_future();
        // RAII
        std::unique_lock<std::mutex> locker(m_task_mtx_);

        // 最多等待1秒，否则任务提交失败
        if (!m_cond_not_full_.wait_for(locker, std::chrono::seconds(1), [&]() -> bool { return m_task_size_.load(std::memory_order_acquire) < m_thresh_hold_task_size_; })) {
            locker.unlock();
            std::cerr << "task queue is full, submit task fail\n";
            logger::log_debug("task queue is full, submit task fail");
            auto emptyTask = std::make_shared<std::packaged_task<ResultType()>>(
              []() -> ResultType { return ResultType(); });
            // 返回给用户一个默认值 表示 任务提交失败
            return emptyTask->get_future();
        }
        // 任务队列有空余位置，加入任务
        // 类型擦除成了 std::function<void()>
        m_task_que_.emplace([task_ptr] { (*task_ptr)(); });
        m_task_size_.fetch_add(1, std::memory_order_release);

        // 通知有新的任务了，使用条件变量通知等待在empty上的消费者线程（线程池的线程）
        m_cond_not_empty_.notify_one();

        // cached模式：任务处理紧急，并且小而且快的任务，根据任务和线程数动态调整线程的数量
        if (m_mode_ == TPoolMode::CACHED && m_task_size_ > m_idle_size_ && m_cur_size_ < m_thresh_hold_task_size_)
        {
            std::cout << "createThreadId[" << std::this_thread::get_id() << "]\n";
            // 添加新线程
            auto cur_thread = std::make_unique<HncThread>([this](const int thread_id) -> void {
                // 这里的线程一定是可变模式，所以直接执行cached_func即可
                this->m_cached_func(thread_id);
            });
            auto cur_tid = cur_thread->get_thread_id();
            m_threads_.emplace(cur_tid, std::move(cur_thread));

            // 启动新线程
            m_threads_[cur_tid]->start();
            m_cur_size_.fetch_add(1, std::memory_order_release); // 当前线程总数 + 1
            m_idle_size_.fetch_add(1, std::memory_order_release);// 空闲线程数 + 1
        }

        return result;
    }

    /**
     * @brief 判断是否为固定线程数线程池
     */
    bool is_fixed() const noexcept;

    /**
     * @brief 打印输出线程池状态
     */
    void print_status() const noexcept;

    /**
     * @brief 调整线程池中任务数量上限,
     * @param task_thresh_hold 任务上限阈值
     * @return 线程池已启动则返回false
     */
    bool set_task_thresh_hold(size_t task_thresh_hold) noexcept;

private:

    // 禁止线程池拷贝构造和赋值
    HncThreadPool(HncThreadPool&) = delete;
    HncThreadPool(HncThreadPool&&) = delete;
    HncThreadPool& operator=(HncThreadPool&) = delete;
    HncThreadPool& operator=(HncThreadPool&&) = delete;


    /**
     * @brief 提供给线程运行 的  固定数量线程函数
     */
    void m_fixed_func(int threadId) noexcept ;

    /**
     * @brief 提供给线程运行的 可变线程数函数
     */
    void m_cached_func(int threadId) noexcept ;

    /**
     * @brief 检查线程池释放再运行
     */
    bool m_check_running() const noexcept { return m_running_.load(std::memory_order_acquire); }

    /**
     * @brief 再函数外部加锁， 从哈希表中删除线程并退出
     */
    bool m_is_exit(int tid) noexcept;

    /**
     * @brief 获取一个任务 并尝试唤醒其他线程继续获取任务
     */
    void m_get_task(std::function<void()> &task) noexcept;



private:


    std::unordered_map<int, std::unique_ptr<HncThread>> m_threads_; // 存放线程池的线程列表

    // 用 uint8_t 也会对齐到4字节， 除非换一下顺序
    int m_init_size_; // 初始启动线程数
    std::atomic_uint m_cur_size_; // 当前线程池中的线程数
    int m_thresh_hold_thread_size_; // 最多能启动的线程数量
    std::atomic_uint m_idle_size_; // 空闲线程数

    std::queue<std::function<void()>> m_task_que_; // 任务队列
    std::atomic<size_t> volatile m_task_size_;// 任务数量
    size_t m_thresh_hold_task_size_;// 最大任务数

    std::mutex m_task_mtx_;// 任务队列互斥锁
    std::condition_variable m_cond_not_full_;// 任务队列不满，
    std::condition_variable m_cond_not_empty_;// 任务队列非空

    TPoolMode m_mode_;// 线程池模式
    std::atomic_bool m_running_;// 线程运行状态

    std::condition_variable m_cond_exit_;// 析构时使用
};
}
