#include "central_cache.h"
#include "page_cache.h"

#include <freelist.h>

namespace hnc::core::mem_pool::details{
CentralCache CentralCache::_m_central_cache; // central_cache的饿汉单例

// 根据内存块字节数大小 获取适应的 申请页面数量， 即使申请最大的内存块，也最高只会去申请管理128页面的spanlist
size_t PageThreshHold(const size_t align_size) {
    // 获取该内存块一次可以申请的最多内存块数量[2个~512个] [256KB ~ 1B]
    // 对应的字节数后右移页面位数，即是对应的最多可以申请的页面数
    const size_t max_page_count = BlockThreshHold(align_size) * align_size >> constant::PAGE_SHIFT;
    // 即4KB的页面， 右移12位， 8B的内存块最多 8 * 512 = 4096B = 1页面
    // 256KB 的内存块 最多256 * 2 = 512KB， 即128个页面

    // 有些平台一页面 = 8KB， 那么8B内存块的4096B 就不足一个页面了，归到1
    if (max_page_count == 0) {
        return 1;
    }
    return max_page_count;
}

/** 尝试从对应块大小的span_list中 的一些span 中分配block_count数量的align_size的块给thread cache
 *  有可能span_list中的span内的块数量 < block_count, 但是一定会至少分配出一个内存块给tc
 */
size_t CentralCache::alloc_to_thread(void *&start, void *&end, const size_t block_count, const size_t align_size) noexcept {
    // 找到链表，获取是哪一个内存块大小对应的链表（208个不同内存块大小的链表）
    // 函数保证至少可以返回一个内存块
    size_t actual_count = 1; // 实际返回的内存块数

    // 此处可能会有多个线程同时访问同一个index的span list，要加锁
    _m_span_lists[Index(align_size)].lock();

    // 获取到一个保证不为空的span
    Span* span = _m_get_span(_m_span_lists[Index(align_size)], align_size);
    // 注意，只有上述函数内是持有pc锁的，运行到这里后 span的use_count并没有初始化！！！
    assert(span);
    assert(span->_freelist_header);

    // 初始start和end都指向span中的第一个内存块
    start = end = span->_freelist_header;

    // 将end指针后移block_count-1次，若块数不足则提前返回
    for (size_t i = 0; i < block_count - 1 && GetNextAddr(end) != nullptr; ++i) {
        end = GetNextAddr(end);
        ++actual_count;
    }

    // 更新span的freelist 指针
    span->_freelist_header = GetNextAddr(end);
    // 更新该span具体分配了多少内存块出去，回收才会使用这个参数
    span->_use_count += actual_count;
    // 更新完链表后可以对该cc解锁
    _m_span_lists[Index(align_size)].unlock();

    // 将返回的尾节点 next指针置空
    GetNextAddr(end) = nullptr;
    return actual_count;
}

// tc释放的一系列内存块返还给spans (可能是从属于多个span的)
void CentralCache::recover_blocks_to_spans(void *start, const size_t align_size) noexcept {
    // 根据内存块的地址 计算出它对应的页面号， 根据页号 从哈希表中 找到 对应的 span地址， 复杂度即O(1)
    // 计算对应链表序号
    const size_t list_index = Index(align_size);

    // 修改对应链表的span， 先上锁
    _m_span_lists[list_index].lock();

    // 依次遍历start起始的链表， 归还每一个内存块
    while (start != nullptr) {
        // 找到对应span
        const auto span = PageCache::GetInstance().find_span_by_address(start);
        // 归还内存块
        void *next = GetNextAddr(start);
        GetNextAddr(start) = span->_freelist_header;
        span->_freelist_header = next; // 更新span的头节点

        // 当一个span的内存块使用数量归为0时说明这个span的所有内存块都归还， 则将其归还给pc 进行合并
        --span->_use_count;


        if (span->_use_count == 0) {
            // 将这块完整的Span 归还给PageCache
            _m_span_lists[list_index].erase(span);

            // 从这里开始别的线程已经访问不到这块span了，对这个链表解锁， 归还pc
            _m_span_lists[list_index].unlock();

            span->_freelist_header = nullptr;
            span->_next = span->_prev = nullptr;


            // page_cache 可能需要将span进行合并, 加锁
            PageCache::GetInstance().lock();
            PageCache::GetInstance().recover_span_to_page_cache(span);
            PageCache::GetInstance().unlock();

            // 重新对这个链表上锁
            _m_span_lists[list_index].lock();
        }
        // 切换下一个内存块
        start = next;
    }


    // 手动解锁 对应的哈希桶
    _m_span_lists[list_index].unlock();
}

/**
 * ① 有span，且 freelist有内存块
 * ③ 有span，但是freelist已经用完了， 无span， 向pc申请 k 页的 span
 * @return freelist 内至少有一个内存块的span
 */
Span * CentralCache::_m_get_span(SpanList &span_list, const size_t align_size) noexcept {
    // ① 遍历所有span查找是否有不为空的freelist，找到即返回该span中的
    for (auto it = span_list.begin(); it != span_list.end(); ++it) {
        if (it->_freelist_header != nullptr) {
            logger::log_debug("thread cache {empty} -> central cache {not empty}, block_size=" + std::to_string(align_size));
            return *it;
        }
    }

    // 在这里一个tc 在cc中找不到可用的资源， 可以先把cc的锁释放掉
    span_list.unlock();

    // ② 有span，但无内存块，和无span一样，从pc申请一个合适大小的span
    // 先获取该内存块最多对应的字节数对应的Page数
    const size_t page_count = PageThreshHold(align_size);

    // 由于span_to_central 函数会递归调用自己，因此在调用这个函数外层手动做加锁和解锁操作，不使用RAII
    PageCache::GetInstance().lock();
    // 从pc中获取一个全新的span 包含了page_count 个页面
    Span *span = PageCache::GetInstance().create_pc_span(page_count);
    logger::log_debug("thread cache {empty} -> central cache {add new span} page_count=" + std::to_string(span->_page_size));
    // 这里还没有释放互斥锁，对于pc的操作是只有一个线程会执行的，因此只要在这一处修改为true即可
    span->_is_use = true;
    span->_block_size = align_size; // 内存块大小
    // 对pc的操作结束，释放pc的锁
    PageCache::GetInstance().unlock();

    // 先获取该span的起始地址和结束地址(真正的结束地址)
    auto start = reinterpret_cast<char*>(span->_page_id << constant::PAGE_SHIFT);
    const char* const end = start + (span->_page_size << constant::PAGE_SHIFT);

    // 将该空间起始地址填入span的freelist
    span->_freelist_header = static_cast<void*>(start);

    // 初始化span块，填充为一个链表
    void* tail = start;
    start += align_size;
    while (start < end) {
        // tail内存块设为下一个内存块的地址
        GetNextAddr(tail) = start;
        // tail 设为下一个内存块的地址
        tail = GetNextAddr(tail);
        start += align_size;
    }
    // 将最后一个内存块的内容设为nullptr，表示自由链表的结束
    GetNextAddr(tail) = nullptr;


    // 对cc的桶操作前再重新对cc上锁, 函数调用方会负责解锁
    span_list.lock();
    // 将该链表放入对应的span_list中
    span_list.insert(*span_list.begin(), span);
    return span;
}



}
