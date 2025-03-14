#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#include "hnc_log.h"


using namespace hnc::core::logger;

void test_log_st() {
    std::cout << "=== single thread log test ===" << std::endl;
    log_info("info log bala bala");
    log_debug("debug apple");
    log_warn("warn orange");
    log_error("error banana");
    log_critical("critical mango");
    log_fatal("tty falut!!!");
}

void test_log_mt() {
    constexpr int THREAD_COUNT = 4;
    constexpr int LOG_COUNT = 10;

    std::cout << "=== mutli thread log test ===" << std::endl;

    std::vector<std::thread> threads;
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < LOG_COUNT; ++i) {
                log_info("thread " + std::to_string(t) + " - msg " + std::to_string(i));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }
}

void test_log_st_large() {
    constexpr int LOG_COUNT = 5000; // 缓冲区应该会置换4次
    std::cout << "=== large log test ===" << std::endl;

    for (int i = 0; i < LOG_COUNT; ++i) {
        log_info("un hahahhahahahahaha. ooooo eee " + std::to_string(i));
    }
}



int main() {
    test_log_st(); // 6条
    test_log_mt(); // 40 条
    test_log_st_large(); // 5000 条

    std::cout << "=== test over! check log/test_log ===" << std::endl;
    return 0;
}
