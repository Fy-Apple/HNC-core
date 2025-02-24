#pragma once

#include "common.h"
#include "freelist.h"

#include <cstddef>


namespace hnc::core::mem_pool::details {
class ThreadCache {
public:
    // 分配size大小的内存块
    void* allocate(size_t size) noexcept;

    // 释放指定地址的内存, 这里的size已经时对齐过的
    void deallocate(void* obj, size_t align_size) noexcept;

private:
    // freelist中没有空闲空间时尝试从CentralCache中获取内存块
    void* _m_alloc_from_central(size_t index, size_t align_size) noexcept;

    // list 回收 内存块大小为size的内存块，数量为list.apple_count
    void _m_release_block(Freelist &free_list, size_t align_size) const noexcept;

private:
    Freelist _m_free_lists[constant::FREE_LIST_SIZE];
};

// 每个线程都拥有自己独立的 局部线程缓存
static inline thread_local ThreadCache* tls_thread_cache_ptr_ = nullptr;
}

/**
[1,128]                 8B对齐            freelist[0,16)
[128+1,1024]            16B对齐           freelist[16,72)
[1024+1,8*1024]         128B对齐          freelist [72,128)
[8*1024+1,64*1024]      1024B对齐         freelist [128,184)
[64*1024+1,256*1024]    8*1024B对齐       freelist[184,208)
*/

