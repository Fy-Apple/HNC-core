#pragma once


namespace hnc::core::thread_pool::details {

/**
 * @brief CRTP 任务基类
 */
template <typename Derived>
class HncTask {
public:
    // CRTP: 默认调用子类的 operator()
    void operator()() { static_cast<Derived*>(this)(); }

};

}