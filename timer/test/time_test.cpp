#include "hnc_timer.h"
#include <iostream>
#include <atomic>

using namespace hnc::core::timer::details;
using namespace hnc::core::logger;



int main() {
    change_log_file_name("timer/benchmark");
    {
        // 获取定时器的两个接口
        auto timer_manager = get_timer_manager(4);;

        // 这样也可以， 需要自己调用start
        // auto timer_manager = get_timer_manager_without_start();
        // timer_manager->start(4);


        std::atomic<int> repeat_counter{0};

        timer_manager->add_timer(std::chrono::seconds(2), [] {
            std::cout << "[一次性任务] 2秒后触发成功！" << std::endl;
        });

        const int repeating_timer_id = timer_manager->add_timer(std::chrono::seconds(1), [&repeat_counter] {
            int count = ++repeat_counter;
            std::cout << "[周期性任务] 第 " << count << " 次触发。" << std::endl;
        }, true);

        timer_manager->add_timer(std::chrono::seconds(3), [] {
            std::cout << "[一次性任务] 3秒后触发成功！" << std::endl;
        });

        timer_manager->add_timer(std::chrono::seconds(4), [] {
            std::cout << "[一次性任务] 4秒后触发成功！" << std::endl;
        });

        timer_manager->add_timer(std::chrono::seconds(5), [&] {
            std::cout << "[删除任务] 删除周期性任务，当前触发次数: " << repeat_counter.load() << std::endl;
            timer_manager->remove_timer(repeating_timer_id);
        });

        std::this_thread::sleep_for(std::chrono::seconds(10));

    }
    return 0;
}
