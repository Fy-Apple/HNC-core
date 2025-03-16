#pragma once

#include <functional>

namespace hnc::core::thread_pool::details {

class HncThread {
public:
    // 线程池底层运行的线程
    explicit HncThread(std::function<void(int)> &&func);
    ~HncThread() = default;

    // CPP17 防止忽略返回值， CPP20支持自定义消息
    [[nodiscard("The thread ID has been ignored!")]] int get_thread_id() const noexcept { return m_threadId_; }

    /**
     * @brief 启动一个线程并 detach 掉
     */
    void start() const noexcept;

private:
    HncThread() = delete;
    HncThread(const HncThread &) = delete;
    HncThread(HncThread &&) = delete;
    HncThread& operator=(const HncThread &) = delete;
    HncThread& operator=(HncThread &&) = delete;

    std::function<void(int)> m_func_; // 线程运行函数
    static int m_generateId_;
    int m_threadId_; // 自定义线程序号
};

inline int HncThread::m_generateId_ = 0;
}