#include "freelist.h"

#include <assert.h>

#include "hnc_log.h"

namespace hnc::core::mem_pool::details {
bool Freelist::empty() const noexcept {
    return _m_freelist_header == nullptr;
}

size_t Freelist::apply_count() const noexcept {
    return _m_apply_count;
}

void Freelist::increment() noexcept {
    // 下次申请的内存块数量
    ++_m_apply_count;
    logger::log_trace("apply_count=" + std::to_string(_m_apply_count));
}

size_t Freelist::size() const noexcept {
    return _m_size;
}

void Freelist::push_front(void* obj) noexcept {
    assert(obj != nullptr);
    GetNextAddr(obj) = _m_freelist_header;
    _m_freelist_header = obj;
    // 内存块+1
    ++_m_size;
    logger::log_trace("add block=1, size=" + std::to_string(_m_size));
}

void Freelist::push_range(void *start, void *end, const size_t size) noexcept {
    // 让end 指向 原header, ==> header 指向新的二 start
    GetNextAddr(end) = _m_freelist_header;
    _m_freelist_header = start;
    _m_size += size;
    logger::log_trace("adds block=" + std::to_string(size) + ", size = " + std::to_string(_m_size));
}
/**
 * 将头部的block_count个内存块回收
 *
 */
void Freelist::pop_range(void *&start, void *&end, size_t block_count) noexcept {
    assert(block_count <= _m_size);
    // 更新freelist的新容量
    _m_size -= block_count;
    // 初始化都指向第一个内存块
    start = end = _m_freelist_header;
    // 找到弹出的最后一个内存块
    while (--block_count) {
        end = GetNextAddr(end);
    }
    // 新起始地址即最后一个内存块内的地址
    _m_freelist_header = GetNextAddr(end);
    GetNextAddr(end) = nullptr;
    logger::log_trace("recycle block=" + std::to_string(block_count) + ", size = " + std::to_string(_m_size));
}

void* Freelist::pop_front() noexcept {
    assert(_m_freelist_header != nullptr);
    void* obj = _m_freelist_header;
    _m_freelist_header = GetNextAddr(obj);
    // 内存块-1
    --_m_size;
    logger::log_trace("recycle block=1, size = " + std::to_string(_m_size));
    return obj;
}
}