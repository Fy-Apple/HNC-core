#pragma once

#include <cstddef>

// 自由链表 管理多个定长内存块
namespace hnc::core::mem_pool::details {

inline void*& GetNextAddr(void* obj) {
    return *static_cast<void**>(obj);
}

class Freelist {
public:
    // 判断自由链表是否为空
    bool empty() const noexcept;

    size_t apply_count() const noexcept;

    void increment() noexcept;

    size_t size() const noexcept;

    /**  将一个内存块归还给自由链表*/
    void push_front(void* obj) noexcept;
    /** 将一段区域的内存块加入自由链表头部 */
    void push_range(void* start, void*end, size_t size) noexcept;
    /** 将头部的block_count个内存块回收 */
    void pop_range(void*& start, void*& end, size_t block_count) noexcept;
    /** 从自由链表头部移除一个内存块返回用户 */
    void* pop_front() noexcept;
private:
    void* _m_freelist_header{nullptr};
    size_t _m_apply_count{1}; // 当前自由链表一次可申请的内存块数，但不会超过thresh_hold阈值
    size_t _m_size{};
};

}
