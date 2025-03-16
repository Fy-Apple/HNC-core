
#include "hnc_timer.h"

namespace hnc::core::timer::details {


HncTimer::HncTimer(std::chrono::milliseconds duration, std::function<void()> callback, bool repeat)
    : task_(std::move(callback), repeat) {
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd_ == -1) {
        throw std::runtime_error("Failed to create timerfd");
    }

    struct itimerspec new_value{};
    new_value.it_value.tv_sec = duration.count() / 1000;
    new_value.it_value.tv_nsec = (duration.count() % 1000) * 1000000;
    if (repeat) {
        new_value.it_interval = new_value.it_value;
    }
    timerfd_settime(timer_fd_, 0, &new_value, nullptr);
}

Timer::~Timer() {
    close(timer_fd_);
}

int Timer::get_fd() const {
    return timer_fd_;
}

void Timer::execute_task() {
    uint64_t expirations;
    read(timer_fd_, &expirations, sizeof(expirations));
    task_.execute();
}

bool Timer::is_repeating() const {
    return task_.is_repeating();
}

}
