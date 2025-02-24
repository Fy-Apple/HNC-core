#pragma once

#include "thread_cache.h"
#include "page_cache.h"

#include <assert.h>


namespace hnc::core::mem_pool {
/**
 *  全局申请内存接口
 */
inline void* tnc_malloc(const size_t size) {
    // 少于MAX_ALLOC_BYTES的字节申请向线程局部缓存申请
    if (size <= details::constant::MAX_ALLOC_BYTES) {
        if (details::tls_thread_cache_ptr_ == nullptr) {
            // 初始化线程局部缓存, 使用定长线程池，内部使用brk系统调用
            static details::FixedMemPool<details::ThreadCache> tc_pool;
            // // 这里不把锁的逻辑放在New里面是因为， span也会需要加锁，这就邮箱效率了，而span的New 已经保证了线程安全
            tc_pool.lock();
            details::tls_thread_cache_ptr_ = tc_pool.New();
            tc_pool.unlock();
        }
        return details::tls_thread_cache_ptr_->allocate(size);
    }
    // 大于MAX_ALLOC_BYTES 直接找pc要
    details::PageCache::GetInstance().lock();
    const details::Span* span = details::PageCache::GetInstance().create_pc_span(details::RoundUp(size) >> details::constant::PAGE_SHIFT);
    details::PageCache::GetInstance().unlock();
    return reinterpret_cast<void*>(span->_page_id << details::constant::PAGE_SHIFT);
}

/**
 *  全局释放内存接口
 */
inline void tnc_free(void* obj) {
    assert(obj);

    // 通过地址查找到对应的span，内部存储了该span所属的内存块大小
    const auto span = details::PageCache::GetInstance().find_span_by_address(obj);
    const size_t size = span->_block_size;

    // 大内存直接通过pc释放
    if (size > details::constant::MAX_ALLOC_BYTES) {
        details::PageCache::GetInstance().lock();
        details::PageCache::GetInstance().recover_span_to_page_cache(span);
        details::PageCache::GetInstance().unlock();
        return;
    }
    details::tls_thread_cache_ptr_->deallocate(obj, size);
}
}