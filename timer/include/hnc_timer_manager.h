#pragma once

#include "hnc_timer_d.h"
#include <unordered_map>
#include <mutex>


namespace hnc::core::timer::details {
/**
 * @brief 负责管理多个定时器
 */
class TimerManager {
public:
    int add_timer(std::chrono::milliseconds duration, std::function<void()> callback, bool repeat = false);
    void remove_timer(int timer_fd);
    HncTimerTask* get_task(int timer_fd);

private:
    std::unordered_map<int, HncTimerTask*> tasks_;
    std::mutex mutex_;
};


}