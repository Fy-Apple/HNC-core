#pragma once

#include "common.h"
#include "span.h"

namespace hnc::core::mem_pool::details {
class CentralCache {
public:
    // 全局cc单例接口
    static CentralCache& GetInstance() noexcept {
        return _m_central_cache;
    }

    // 尝试从对应块大小的span_list中 的一些span 中分配block_count数量的align_size的块给thread cache
    // 有可能span_list中的span内的块数量 < block_count, 但是一定会至少分配出一个内存块给tc
    size_t alloc_to_thread(void*& start, void*& end, size_t block_count, size_t align_size) noexcept;

    // tc释放的一系列内存块返还给spans (可能是从属于多个span的)
    void recover_blocks_to_spans(void* start, size_t align_size) noexcept;


private:
    CentralCache() = default;
    ~CentralCache() = default;

    CentralCache(const CentralCache&) = delete;
    CentralCache(CentralCache&&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;
    CentralCache& operator=(CentralCache&&) = delete;

    // 返回一个内部freelist至少包含一个内存块的span
    Span* _m_get_span(SpanList &span_list, size_t align_size) noexcept;

private:
    // 组织span的208个不同块大小的span的双向链表，每个span内部又有一个freelist，
    SpanList _m_span_lists[constant::FREE_LIST_SIZE];
    // 必须在cpp中初始化，否则每个翻译单元包含一个static，违背ODR原则，重复定义编译报错
    static CentralCache _m_central_cache;
};
}
