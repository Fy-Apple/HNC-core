#pragma once

#include <atomic>

namespace hnc::core::thread_pool::details {
// 线程池可选择固定数量线程的模式，或 可变模式
enum class TPoolMode {
    FIXED,
    CACHED,
};

namespace constant {
constexpr size_t INIT_THREAD_SIZE = 4; // 线程池启动时初始线程数
constexpr size_t THREAD_HOLD_THREAD_SIZE = 10; // 最大可存在线程数 通常可以设为CPU核心线程数少一点点
constexpr size_t THRESH_HOLD_TASK_SIZE = 1024; // 任务队列最大任务数量
constexpr size_t THREAD_IDLE_TIME = 8; // 可变模式下空闲线程多久回收自己
}

}