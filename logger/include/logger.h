#pragma once
#include <string>
#include <source_location>
#include <shared_mutex>

#include "log_thread.h"


namespace hnc::core::logger::details {
class Logger{
public:
    // 全局懒汉单例
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger(Logger &&) = delete;

    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger &&) = delete;

    /**
     * @brief 多生产者写入日志 ， 主从缓冲区切换
     * @param msg 封装好的日志内容
     */
    void log(const std::string& msg) const noexcept;

private:

    Logger();
    ~Logger();


    // 类似于mysql的 元数据锁， 生产者crud时不互斥， 交换缓冲区结构时互斥
    mutable std::shared_mutex m_meta_mtx_;
    LogThread m_log_thread_; // 日志线程
};
}
