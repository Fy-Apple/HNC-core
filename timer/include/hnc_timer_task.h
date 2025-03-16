#pragma once

#include <functional>

/**
 * @brief 任务类，封装定时任务
 */
class HncTimerTask {
public:
    explicit HncTimerTask(std::function<void()> callback, const bool repeat = false)
        : callback_(std::move(callback)), repeat_(repeat) {}
    HncTimerTask() : callback_([] {}), repeat_(false) {}
    void execute() const {
        if (callback_) callback_();
    }

    bool is_repeating() const { return repeat_; }

private:
    std::function<void()> callback_;  // 任务回调
    bool repeat_;                     // 是否是周期任务
};



/**
 * @brief 定时任务封装
 */
class HncTimerTask {
public:
    HncTimerTask() = default;
    explicit HncTimerTask(std::function<void()> callback, bool repeat = false)
        : callback_(std::move(callback)), repeat_(repeat) {}

    void execute() const { if (callback_) callback_(); }
    bool is_repeating() const { return repeat_; }

private:
    std::function<void()> callback_;
    bool repeat_ = false;
};
