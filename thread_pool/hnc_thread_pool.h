#pragma once

#include "thread_pool.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace hnc::core::thread_pool {

class ThreadPoolManager {
public:

    // 禁止拷贝 & 赋值
    ThreadPoolManager(const ThreadPoolManager&) = delete;
    ThreadPoolManager& operator=(const ThreadPoolManager&) = delete;
    ThreadPoolManager(ThreadPoolManager&&) = delete;
    ThreadPoolManager& operator=(ThreadPoolManager&&) = delete;

    /**
     * @brief 获取固定大小线程池
     * @param name 线程池名称
     * @param thread_count 线程数
     * @return std::shared_ptr<HncThreadPool>
     */
    static std::shared_ptr<details::HncThreadPool> get_fixed_pool(const std::string& name, const size_t thread_count = details::constant::INIT_THREAD_SIZE) {
        return m_get_pool(name, details::TPoolMode::FIXED, thread_count);
    }

    /**
     * @brief 获取可变大小线程池
     * @param name 线程池名称
     * @param init_threads 初始线程数
     * @return std::shared_ptr<HncThreadPool>
     */
    static std::shared_ptr<details::HncThreadPool> get_cached_pool(const std::string& name, const size_t init_threads = details::constant::INIT_THREAD_SIZE) {
        return m_get_pool(name, details::TPoolMode::CACHED, init_threads);
    }

private:
    ThreadPoolManager() = default;  // 私有构造，单例模式

    /**
     * @brief 统一线程池管理
     * @param name 线程池名称
     * @param mode 线程池模式（Fixed 或 Cached）
     * @param thread_count 初始线程数
     * @return std::shared_ptr<HncThreadPool>
     */
    static std::shared_ptr<details::HncThreadPool> m_get_pool(const std::string& name, details::TPoolMode mode, const size_t thread_count) {
        std::lock_guard<std::mutex> lock(m_mtx_);
        const auto it = m_thread_pools_.find(name);
        if (!m_thread_pools_.contains(name)) {
            // 第一次创建新的线程池
            auto pool = std::make_shared<details::HncThreadPool>(mode, thread_count);
            pool->start();
            //thread_pools_[name] = pool;
            return pool;
        }
        return it->second;  // 返回已存在的线程池
    }

    static std::mutex m_mtx_;  // 线程安全锁
    static std::unordered_map<std::string, std::shared_ptr<details::HncThreadPool>> m_thread_pools_;  // 线程池管理
};

inline std::mutex ThreadPoolManager::m_mtx_;
inline std::unordered_map<std::string, std::shared_ptr<details::HncThreadPool>> ThreadPoolManager::m_thread_pools_;


}
