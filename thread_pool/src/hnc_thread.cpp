#include "hnc_thread.h"

#include <thread>

namespace hnc::core::thread_pool::details {

HncThread::HncThread(std::function<void(int)> &&func) : m_func_(func), m_threadId_(++m_generateId_){
    // 线程编号从 1 开始


}

/**
 * @brief 启动一个线程并 detach 掉
 */
void HncThread::start() const noexcept {
    std::thread t(m_func_, m_threadId_);
    t.detach();
}
}
