#include "page_cache.h"

namespace hnc::core::mem_pool::details {
PageCache PageCache::_m_page_cache; // _m_page_cache的全局饿汉单例

/** 将page_count数量的span 返回给central_cache
 *  1. 先检查自己对应的哈希桶中是否有空闲的span，有则返回
 *  2. 若没有则，继续向上查找，是否有空闲span，有则分割后返回
 *  3. 若所有哈希桶中均没有空闲span，则向OS申请一篇足够大的span分割后挂载到对应list中返回
 */
Span * PageCache::create_pc_span(const size_t page_count) noexcept {
    assert(page_count > 0);
    // <=256KB的会走thread_local
    // 256 ~ 512 会走 pc
    // >= 512 直接走系统的mmap给
    // 超过512KB的申请，即超过128Page增加新的逻辑，这里的默认Page为4KB
    if (page_count > constant::MAX_PAGE_COUNT) {
        auto ptr = SystemAlloc(page_count);
        const auto span = _m_span_pool.New();

        span->_page_id = reinterpret_cast<size_t>(ptr) >> constant::PAGE_SHIFT;
        span->_page_size = page_count;
        span->_block_size = constant::MAX_ALLOC_BYTES + 1;
        _m_page_span_map[span->_page_id] = span;
        _m_page_span_map[span->_page_id + span->_page_size - 1] = span;
        logger::log_debug("page cache {big block} -> os , page_count=" + std::to_string(page_count));
        return span;
    }
    // 1B ~ 256KB ~ 1024KB 即1Page ~ 32page ~ 128Page，
    // -1 对应数组中的下标
    const int list_index = page_count - 1;
    // 1. 先检查自己对应的哈希桶中是否有空闲的span，有则返回
    if (!_m_span_lists[list_index].empty()) {
        Span * span = _m_span_lists[list_index].pop_front();

        // 更新分配出去的span和页号的哈希
        for (size_t i = 0; i < span->_page_size; ++i) {
            _m_page_span_map[span->_page_id + i] = span;
        }
        logger::log_debug("thread cache {empty} -> central cache {empty} -> page cache {not empty}, page_count=" + std::to_string(page_count));
        return span;
    }

    // 2. 若没有则，继续向上查找，是否有空闲span，有则分割后返回
    for (int i = list_index; i < constant::MAX_PAGE_COUNT; ++i) {
        if (!_m_span_lists[i].empty()) {
            // 取出该span
            Span *complete_span = _m_span_lists[i].pop_front();

            // 动态申请一个新的span，将该span分割
            auto *prev_span = _m_span_pool.New();

            // 新span的页号 = 老span的页号， 页数 = 传入参数page_count
            prev_span->_page_id = complete_span->_page_id;
            prev_span->_page_size = page_count;
            // 老span页号 + page_count = 新页号, 页数减去page_count
            complete_span->_page_id += page_count;
            complete_span->_page_size -= page_count;

            // 将老span放回对应的span_list中
            _m_span_lists[complete_span->_page_size - 1].push_front(complete_span);

            // 考虑极端情况
            // 申请的list_index 是0， 那么会跳过当前for，申请一个128页的span，一定会到这里分割
            // 变成1page的span和127page的span， 那么这个分割后的page_span只需要记录两端页号即可
            // 下次再来一个新的，也只会走这里，

            // TODO: 替换 `_m_page_span_map` 为基数树（Radix Tree），避免 `unordered_map` 触发 operator new 递归 死锁
            //  `std::unordered_map` 在插入新元素或 rehash 时，会调用 `operator new` 分配新内存。
            //  如果全局 `operator new` 被重载，它可能会 递归调用 unordered_map.insert()，导致 卡死在这里
            //  解决方案：用 基数树（Radix Tree） 代替 `unordered_map`，避免 `operator new` 的干扰。

            _m_page_span_map[complete_span->_page_id] = complete_span;
            _m_page_span_map[complete_span->_page_id + complete_span->_page_size - 1] = complete_span;
            // std::cout << 'update\n';

            // 更新分配出去的span和页号的哈希
            for (size_t j = 0; j < prev_span->_page_size; ++j) {
                _m_page_span_map[prev_span->_page_id + j] = prev_span;
            }
            logger::log_debug("thread cache {empty} -> central cache {empty} -> page cache {not empty}, split=" + std::to_string(page_count)  + ", " + std::to_string(i + 1));
            return prev_span;
        }
    }


    // 3. 若所有哈希桶中均没有空闲span，则向OS申请一篇足够大的span分割后挂载到对应list中返回
    void* mem_ptr = SystemAlloc(constant::MAX_PAGE_COUNT);
    logger::log_debug("thread cache {empty} -> central cache {empty} -> page cache {empty} -> os {span(128 page)}");

    // 动态申请一个新的span，
    auto *span = _m_span_pool.New();

    // 新span的页号 = 老span的页号， 页数 = 传入参数page_count
    span->_page_id = reinterpret_cast<size_t>(mem_ptr) >> constant::PAGE_SHIFT;
    span->_page_size = constant::MAX_PAGE_COUNT;

    // 将这个span放回span_list
    _m_span_lists[constant::MAX_PAGE_COUNT - 1].push_front(span);
    // 递归调用自己
    return create_pc_span(page_count);
}

// 根据地址再unordered_map中查找对应的span
Span * PageCache::find_span_by_address(void *addr) noexcept {
    std::unique_lock guard(_m_mtx);
    if (const size_t page_id = reinterpret_cast<size_t>(addr) >> constant::PAGE_SHIFT; _m_page_span_map.contains(page_id)) {
        return _m_page_span_map[page_id];
    }
    // 不应该到达这里
    assert(false);
    return nullptr;
}

// 回收一个span， 并尝试合并到更大的page对应的span_list中
void PageCache::recover_span_to_page_cache(Span *span) noexcept {

    // 超过128页面的span直接归还OS
    if (span->_page_size > constant::MAX_PAGE_COUNT) {
        SystemFreeMMap(reinterpret_cast<void*>(span->_page_id << constant::PAGE_SHIFT), span->_page_size);
        // 从定长内存池中删除span(归还定长内存池)

        _m_span_pool.Delete(span);
        logger::log_debug("free to pc(os) ,page_size=" + std::to_string(span->_page_size));
        return;
    }

    // 合并左侧span
    while (true) {
        const size_t left_page_id = span->_page_id - 1;
        // 没有该span的相邻左页面则跳过
        if (!_m_page_span_map.contains(left_page_id))
            break;
        const auto left_span = _m_page_span_map[left_page_id];
        // 有相邻左页面，但是正在被cc使用则跳过
        // (这里必须使用isUse这个在pc锁内就被改的变量，而不能使用use_count),
        // 因为use_count 在一个span刚划分出去时也是0， 还没有增加，可能会让其他线程误认为是没有使用的span
        if (left_span->_is_use == true)
            break;
        // 如果两个相邻span合并后超过Page_Num个page也要跳过，因为page_cache无法管理超过Page_Max_Num个页面的Span
        if (left_span->_page_size + span->_page_size > constant::MAX_PAGE_COUNT)
            break;
        // 进行两个span的合并
        span->_page_id = left_span->_page_id;
        span->_page_size += left_span->_page_size;
        // 删除原有的左侧span
        _m_span_lists[left_span->_page_size - 1].erase(left_span);

        // 因为span是new出来的所以需要显式delete, 从定长内存池中删除(归还定长内存池)
        _m_span_pool.Delete(left_span);
        logger::log_debug("free to pc ,left merge=" + std::to_string(span->_page_size));
    }
    // 合并右侧span， 相同逻辑
    while (true) {
        const size_t right_page_id = span->_page_id + span->_page_size;
        // 没有该span的相邻左页面则跳过
        if (!_m_page_span_map.contains(right_page_id))
            break;
        const auto right_span = _m_page_span_map[right_page_id];
        if (right_span->_is_use == true)
            break;
        if (right_span->_page_size + span->_page_size > constant::MAX_PAGE_COUNT)
            break;
        span->_page_size += right_span->_page_size;
        _m_span_lists[right_span->_page_size - 1].erase(right_span);
        // 因为span是new出来的所以需要显式delete, 从定长内存池中删除(归还定长内存池)
        _m_span_pool.Delete(right_span);
        logger::log_debug("free to pc ,right merge=" + std::to_string(span->_page_size));
    }

    // 合并完成后， 将当前span挂载到对应的哈希桶中
    _m_span_lists[span->_page_size - 1].push_front(span);
    span->_is_use = false; // 回收回page_cache 的span

    // 将当前span的两端页面映射到哈希表上，以供下次合并使用
    _m_page_span_map[span->_page_id] = span;
    _m_page_span_map[span->_page_id + span->_page_size - 1] = span;
    logger::log_debug("free to pc ,span page_size=" + std::to_string(span->_page_size));

}
}
