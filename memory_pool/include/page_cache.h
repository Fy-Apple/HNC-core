#pragma once

#include "common.h"
#include "span.h"
#include "fixed_mem_pool.h"

#include <mutex>
#include <unordered_map>

namespace hnc::core::mem_pool::details {
/**
 *  这里的span要分裂，合并，多个不同的线程可能同时在调用cc的函数操作pc，因此pc要全局锁
 */
class PageCache {
public:
    // 全局唯一page_cache 单例
    static PageCache& GetInstance() {
        return _m_page_cache;
    }

    // 将page_count数量的span 返回给central_cache
    Span* create_pc_span(size_t page_count) noexcept;

    // 根据地址再unordered_map中查找对应的span
    Span* find_span_by_address(void* addr) noexcept;

    // 回收一个完整的span加入到对应的page_span_list中
    void recover_span_to_page_cache(Span *span) noexcept;

    // 提供上锁接口 和 解锁接口
    void lock() noexcept {
        _m_mtx.lock();
    }
    void unlock() noexcept {
        _m_mtx.unlock();
    }

private:
    PageCache() = default;
    ~PageCache() = default;

    PageCache(const PageCache&) = delete;
    PageCache(PageCache&&) = delete;
    PageCache& operator=(const PageCache&) = delete;
    PageCache& operator=(PageCache&&) = delete;
    
private:
    SpanList _m_span_lists[constant::MAX_PAGE_COUNT]; // 按页面数量不同管理不同span_list， 默认是256个页面
    std::mutex _m_mtx; // 对整体加锁
    std::unordered_map<size_t, Span*> _m_page_span_map;

    // span对象的定长内存池
    FixedMemPool<Span> _m_span_pool;

    // page_cache 也是全局唯一单例
    static PageCache _m_page_cache;
};




}

