#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "hnc_thread_pool.h"


/**
 * @brief 一个具体任务，继承 Task<MyTask>
 * : public hnc::core::thread_pool::details::HncTask<MyTask> 并不需要，已经类型擦除了 CRTP不需要使用了
 */
class MyTask {
public:
    void operator()() const noexcept {
        std::cout << "Executing MyTask in thread: " << std::this_thread::get_id() << std::endl;
    }
};


// 简单任务：打印线程 ID
void simple_task(const int id) {
    std::cout << "Task " << id << " executed！\n";
}

// 计算任务
int sum_task(const int a, const int b) {
    return a + b;
}

void test_fixed_pool() {
    std::cout << "======== [Test 1] Fixed 线程池 ========\n";
    const auto poolA = hnc::core::thread_pool::ThreadPoolManager::get_fixed_pool("Fixed_Pool(4)", 1);

    poolA->submit_task(simple_task, 1);
    poolA->submit_task(simple_task, 1);
    poolA->submit_task(simple_task, 1);
    poolA->submit_task(simple_task, 1);


    std::this_thread::sleep_for(std::chrono::seconds(3));
    poolA->print_status();
    std::cout << "======== [Test 1] over ========\n";
}

void test_cached_pool() {
    std::cout << "======== [Test 2] Cached 线程池 ========\n";
    auto cachedPoolX = hnc::core::thread_pool::ThreadPoolManager::get_cached_pool("CacheX", 5);

    for (int i = 0; i < 10; ++i) {
        cachedPoolX->submit_task(simple_task, i);
    }
    std::cout << "submit 10 sample task!\n";

    auto futureSum = cachedPoolX->submit_task(sum_task, 10, 20);
    std::cout << "Result of sum_task(10,20): " << futureSum.get() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));
    cachedPoolX->print_status();

    std::cout << "======== [Test 2] over========\n";
}

void test_fixed_performance() {
    std::cout << "======== [Test 3] Fixed 100 task ========\n";
    const auto poolA = hnc::core::thread_pool::ThreadPoolManager::get_fixed_pool("Fixed_Pool(4)");
    for (int i = 0; i < 100; ++i) {
        const auto success = poolA->submit_task(simple_task, i);
        if (!success.valid()) {
            std::cout << "Task " << i << " submission failed!\n";
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "======== [Test 3] over ========\n";
}

void test_cached_performance() {
    std::cout << "======== [Test 4] Cached 100 task ========\n";
    const auto CacheX = hnc::core::thread_pool::ThreadPoolManager::get_cached_pool("CacheX");
    for (int i = 0; i < 100; ++i) {
        const auto success = CacheX->submit_task(simple_task, i);
        if (!success.valid()) {
            std::cout << "Task " << i << " submission failed!\n";
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::cout << "======== [Test 4] over ========\n";
}

void test_obj_task() {
    std::cout << "======== [Test 5] task obj ========\n";
    MyTask task1;
    MyTask task2;
    const auto pool = hnc::core::thread_pool::ThreadPoolManager::get_fixed_pool("Fixed_Pool(4)", 1);

    pool->submit_task(task1);
    pool->submit_task(task2);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::cout << "======== [Test 5] over ========\n";
}


int main() {
    test_fixed_pool();
    test_cached_pool();
    test_fixed_performance();
    test_cached_performance();
    test_obj_task();
    std::cout << "======== [Test Completed] ========\n";
    return 0;
}
