#include <iostream>
#include <vector>
#include <thread>

#include "tnc_malloc.h"

#include "benchmark.h"



constexpr size_t SMALL_BLOCK = 128;  // 128B 此会走tc线程局部缓存
constexpr size_t MEDIUM_BLOCK = 384 * 1024; // 384KB  此会走pc 但是仍然pc内部的span分配（brk）
constexpr size_t LARGE_BLOCK = 768 * 1024;  // 768KB  此会走pc  但是走的是mmap系统调用

constexpr int NUM_THREADS = 4;
constexpr int ALLOC_COUNT = 10000;  // 每个线程分配的次数
using namespace hnc::core::mem_pool;

//  基础的 malloc 和 tnc_malloc 对比
void benchmark_alloc_free() {
    // 测试小块内存
    {
        TICK(malloc_small_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = malloc(SMALL_BLOCK);
            free(ptr);
        }
        TOCK(malloc_small_block)
    }
    {
        TICK(tnc_malloc_small_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = tnc_malloc(SMALL_BLOCK);
            tnc_free(ptr);
        }
        TOCK(tnc_malloc_small_block)
    }

    // 测试中等内存块
    {
        TICK(malloc_medium_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = malloc(MEDIUM_BLOCK);
            free(ptr);
        }
        TOCK(malloc_medium_block)
    }
    {
        TICK(tnc_malloc_medium_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = tnc_malloc(MEDIUM_BLOCK);
            tnc_free(ptr);
        }
        TOCK(tnc_malloc_medium_block)
    }

    // 测试大块内存（mmap）
    {
        TICK(malloc_large_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = malloc(LARGE_BLOCK);
            free(ptr);
        }
        TOCK(malloc_large_block)
    }
    {
        TICK(tnc_malloc_large_block)
        for (int i = 0; i < ALLOC_COUNT; ++i) {
            void* ptr = tnc_malloc(LARGE_BLOCK);
            tnc_free(ptr);
        }
        TOCK(tnc_malloc_large_block)
    }
}

//  容器测试
void benchmark_vector() {
    // 标准容器
    {
        TICK(vector_int)
        std::vector<int> vec;
        for (int i = 0; i < ALLOC_COUNT; ++i) vec.push_back(i);
        TOCK(vector_int)
    }
    // 使用内存池适配器的标准容器
    {
        TICK(tnc_vector_int)
        std::vector<int, TncAllocator<int>> vec;
        for (int i = 0; i < ALLOC_COUNT; ++i) vec.push_back(i);
        TOCK(tnc_vector_int)
    }
    // pmr容器
    {
        TncMemRe mem;
        TICK(pmr_vector_int)
        std::pmr::vector<int> vec(&mem);
        for (int i = 0; i < ALLOC_COUNT; ++i) vec.push_back(i);
        TOCK(pmr_vector_int)
    }
}

int main() {
    benchmark_alloc_free();
    benchmark_vector();
    return 0;
}