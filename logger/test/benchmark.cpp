#include <iostream>

#include <thread>

#include "benchmark.h"
#include "hnc_log.h"
#include <condition_variable>
using namespace hnc::core::logger;


using namespace hnc::core::logger;

void test_log_mt_performance() {
    constexpr int LOG_COUNT = 10000;
    constexpr int THREAD_COUNT = 16;

    std::cout << "=== multi thread performance test ===" << std::endl;
    std::vector<std::thread> threads;
    std::latch lch(THREAD_COUNT + 1);
    std::latch down(THREAD_COUNT);
    auto func = [&] (const std::string &name) -> void {
        // std::ostringstream oss;
        // oss << std::this_thread::get_id();
        // const auto str = oss.str();

        // 等待所有线程就绪后 开始 打印
        std::cout << name << " ready\n";
        lch.arrive_and_wait();
        std::cout << name << " start\n";
        for (int i = 0; i < LOG_COUNT; ++i) {
            log_info(name + " -> yes no ->" + std::to_string(i));
        }
        // 计数器 - 1
        down.count_down();
    };
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(func, std::to_string(i + 1));
    }
    // 启动！
    lch.count_down();
    TICK(test_log_mt_performance)
    // 等待所有线程完成后统计耗时
    down.wait();
    TOCK(test_log_mt_performance)

    for (auto &t : threads) {
        t.join();
    }
}


int main(int argc, char *argv[]) {
    change_log_file_name("logger/test_log_benchmark");

    test_log_mt_performance();
    return 0;
}
