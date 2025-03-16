#pragma once

#include "hnc_timer_manager.h"


namespace hnc::core::timer::details {

// TODO : 当传入线程数 为 1 时 不去使用线程池， 直接使用一个后台线程 负责监听和执行定时任务

/**
 * @brief 提供一个已经启动的 定时器 模块， 需要指定后台线程数， 不可少于2
 */
inline std::shared_ptr<HncTimerManager> get_timer_manager(const uint8_t threads) {
    auto manager =  std::make_shared<HncTimerManager>();
    manager->start(std::min(static_cast<int>(threads), 2));
    return manager;
}

/**
 * @brief 提供一个延迟启动的 定时器， 需要用户自己调用start 并指定 线程数 start->(xxx);
 */
inline std::shared_ptr<HncTimerManager> get_timer_manager_without_start() {
    return std::make_shared<HncTimerManager>();
}

}
