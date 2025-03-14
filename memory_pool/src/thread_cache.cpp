#include "thread_cache.h"
#include "central_cache.h"

#include <algorithm>
#include <cassert>


namespace hnc::core::mem_pool::details {

void* ThreadCache::allocate(const size_t size) noexcept{
    assert(size <= constant::MAX_ALLOC_BYTES);
    const size_t align_size = RoundUp(size); // 内存对齐字节数
    const size_t list_index = Index(size); // 对应大小的链表序号

    // 若链表内有内存块则从自由链表分配内存
    if (!_m_free_lists[list_index].empty()) {
        logger::log_debug("thread cache -> not empty, return");
        return _m_free_lists[list_index].pop_front();
    }
    // 向centralcache 申请内存，并修改自由链表
    return _m_alloc_from_central(list_index, align_size);
}

/**
 * @param obj 归还内存池的内存地址
 * @param align_size 内存块大小
 * 回收时当一个 free_list 的块数 > 一次可最多申请的内存块时触发回收动作
 * 定义 这一次回收  一次可申请的内存块的数
 */
void ThreadCache::deallocate(void *obj, const size_t align_size) noexcept {
    assert(obj);
    assert(align_size <= constant::MAX_ALLOC_BYTES);
    const size_t list_index = Index(align_size);
    // 将内存块返回对应链表
    _m_free_lists[list_index].push_front(obj);
    logger::log_debug("free to tlc ,block_size=" + std::to_string(align_size));
    // 可用内存块 > 下一次可申请的内存块， 则回收apple_count数量的内存块
    if (_m_free_lists[list_index].size() >= _m_free_lists[list_index].apply_count()) {
        _m_release_block(_m_free_lists[list_index], align_size);
    }
}

void * ThreadCache::_m_alloc_from_central(const size_t index, const size_t align_size) noexcept {

    // 获取本次需要向cc申请的内存块数量(不超过阈值)
    const size_t block_count = std::min(_m_free_lists[index].apply_count(), BlockThreshHold(align_size));
    // 只要没有超过阈值，那么下次在这个类型的freelist申请块数+1，动态增长
    if (block_count == _m_free_lists[index].apply_count()) {
        /** 慢开始调节算法 */
        _m_free_lists[index].increment();
    }

    // 申请到的内存块区域范围 左闭右闭[], start和end都指向一个可以使用的内存块
    void *start, *end;

    // 向cc申请n块大小size的内存块(有可能小于申请的数量，但是一定至少会申请到一个内存块)

    // 申请到的第一个内存块需要返回给线程，剩余的内存块才可加入自由链表，当只申请到一个内存块时，则不用更新tc对应的自由链表
    const size_t actual_count = CentralCache::GetInstance().alloc_to_thread(start, end, block_count, align_size);
    logger::log_debug("thread cache {get cc's blocks} block_count=" + std::to_string(actual_count));
    if (actual_count == 1)
    {
        assert(start == end);
        logger::log_debug("thread cache {to user} align_size=" + std::to_string(align_size));
        return start;
    }

    // 更新自由链表
    _m_free_lists[index].push_range(GetNextAddr(start), end, actual_count - 1);
    logger::log_debug("thread cache {remain block} block_count=" + std::to_string(actual_count - 1));
    return start;
}

// list 回收 内存块大小为size的内存块，数量为list.apple_count
void ThreadCache::_m_release_block(Freelist &free_list, const size_t align_size) const noexcept {
    void *start, *end;
    // 回收指定数量的内存块， 内存块序号为[start -> ... -> ... -> end]
    free_list.pop_range(start, end, free_list.apply_count());
    // 将这串内存块 ( 单向链表 ,且end节点已经指向了nullptr) 归还给cc
    logger::log_debug("free to cc ,block_size=" + std::to_string(free_list.apply_count()));
    CentralCache::GetInstance().recover_blocks_to_spans(start, align_size);
}


}
