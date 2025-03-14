#pragma once

#include "alloc.h"
#include <memory_resource> // for cpp17


/**
 * TODO: 1. 读取全局配置文件，决定使用哪个 malloc 实现
 *  example:
 *  alloc = tc_malloc   // pt_malloc,tc_malloc,je_malloc
 *  new = ...
 *
 *  TODO: 2. 目前全局重载operator new 会导致内部使用的unordered_map 也会走 operator new 陷入死循环，
 *   后续需要替换unordered_map 或者寻找其他解决方法
*/

/**
 * @brief 重载:: operator new, 只负责分配内存， new才是负责调用构造的
 *
 * @param size 需要分配的字节数
 * @return void* 分配的内存指针
 *
 */
// inline void* operator new(size_t size) {
//     std::cout << "operator new!\n";
//     return hnc::core::mem_pool::tnc_malloc(size);
// }

/**
 * @brief 重载 ::operator delete，只负责释放内存，delete才是负责调用析构的
 *
 * @param ptr 释放的内存地址
 *
 * @note 大块内存使用PC申请的大内存
 */
// inline void operator delete(void* ptr) noexcept {
//     std::cout << "operator delete!\n";
//     hnc::core::mem_pool::tnc_free(ptr);
// }

/**
 * @brief C++14 及以上可能会调用带 size_t 参数的 delete
 */
// inline void operator delete(void* ptr, size_t size) noexcept {
//     std::cout << "operator delete with size!\n";
//     hnc::core::mem_pool::tnc_free(ptr);
// }

// 普通对象 继承此类重载operator new
class TncMemObj {
public:
    // 在头文件中定义的类方法 默认就有inline修饰符了
    void* operator new(size_t size) {
        hnc::core::logger::log_trace("operator new !");
        return hnc::core::mem_pool::tnc_malloc(size);
    }

    void operator delete(void* ptr) noexcept {
        hnc::core::logger::log_trace("operator delete !");
        hnc::core::mem_pool::tnc_free(ptr);
    }
};

// 标准容器 使用 自定义空间适配器
template <typename T>
struct TncAllocator {
    using value_type = T;
    TncAllocator() = default;
    // 分配内存
    T* allocate(std::size_t size) {
        hnc::core::logger::log_trace("TncAllocator alloc !");
        return static_cast<T*>(hnc::core::mem_pool::tnc_malloc(size * sizeof(T)));
    }

    // 释放内存
    void deallocate(T* p, std::size_t) noexcept {
        hnc::core::logger::log_trace("TncAllocator dealloc !");
        hnc::core::mem_pool::tnc_free(p);
    }
};

// pmr容器使用该上游内存资源
class TncMemRe : public std::pmr::memory_resource {
protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        hnc::core::logger::log_trace("pmr alloc !");
        return hnc::core::mem_pool::tnc_malloc(bytes);
    }

    void do_deallocate(void* p, std::size_t, std::size_t) override {
        hnc::core::logger::log_trace("pmr dealloc !");
        hnc::core::mem_pool::tnc_free(p);
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};
