#pragma once
#include <cassert>
#include <cstddef>
#include <new>

#ifdef _WIN32
#include<Windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#endif

#include <cstdlib>



namespace hnc::core::mem_pool {

// TODO : 后续需要将可配置参数全部移入 配置文件读取 实例
namespace details::constant {
/**
 * 默认按照一个页面4K 256 * 4K = 1M， 默认一个块256KB， 即最大span已经 = 4个块了，足矣
 */
inline constexpr int FREE_LIST_SIZE = 208; // 一共可以对应208个自由链表
inline constexpr int MAX_ALLOC_BYTES = 256 * 1024; // 一次可分配最大内存块 256KB
inline constexpr int MAX_PAGE_COUNT = 128; // PageCache中的一个span最多可以包含的页面数
inline constexpr int PAGE_SHIFT = 12; // 2^12 = 4096, 一页4KB

}

namespace details {
/**
 * 超过128个page的内存块， 直接由MMAP系统调用去映射，  使用匿名 anonymous   文件描述符设为-1即不映射文件
*/
inline void* SystemAllocMMap(const size_t page_count) {
#ifdef _WIN32
    void* mem_ptr = VirtualAlloc(0, page_count << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // 由OS决定从哪里分配， 读写， 不影响其他进程， 不与文件关联
    void* mem_ptr = mmap(nullptr, page_count << constant::PAGE_SHIFT, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    if (mem_ptr == nullptr) {
        throw std::bad_alloc();
    }
    return mem_ptr;
}
/**
 * 用于释放由mmap申请的大块内存块 （ > 512 KB 的内存块）
*/
inline void SystemFreeMMap(void* ptr, const size_t page_count) {
    munmap(ptr,page_count << constant::PAGE_SHIFT);
}


inline void* SystemAlloc(const size_t page_count)
{
    if (page_count > constant::MAX_PAGE_COUNT) {
        return SystemAllocMMap(page_count);
    }
#ifdef _WIN32
    void* ptr = VirtualAlloc(0, page_count << constant::PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    // 使用brk系统调用调整进程堆尾指针来扩展堆内存
    void *ptr = sbrk(0);
    if (brk((sbrk(0) + (page_count << constant::PAGE_SHIFT))) == -1) {
        perror("brk failed");
        ptr = nullptr;
    }
#endif
    if (ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

inline size_t _RoundUp(const size_t size, const size_t alignment) noexcept {
    return (size + alignment - 1) & ~ (alignment - 1);
}

// 将申请字节对齐
inline size_t RoundUp(const size_t size) noexcept {
    if (size >= 1 && size <= 128)
        return _RoundUp(size, 8);
    if (size <= 1024)
        return _RoundUp(size, 16);
    if (size <= 8 * 1024)
        return _RoundUp(size, 128);
    if (size <= 64 * 1024)
        return _RoundUp(size, 1024);
    if (size <= 256 * 1024)
        return _RoundUp(size, 8 * 1024);
    // 超出256KB的大量 直接对齐到一个Page
    return _RoundUp(size, 1 << constant::PAGE_SHIFT);
}
// 获取对应字节数所属的自由链表序号
inline size_t _Index(const size_t size, const size_t align_shift) noexcept
{							/*这里align_shift是指对齐数的二进制位数。比如size为2的时候对齐数
                            为8，8就是2^3，所以此时align_shift就是3*/
    return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    //这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
}

// 计算映射的哪一个自由链表桶
inline size_t Index(const size_t size) noexcept
{
    assert(size <= constant::MAX_ALLOC_BYTES);

    // 每个区间有多少个链
    static int group_array[4] = { 16, 56, 56, 56 };
    if (size <= 128)
        // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
            return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
    if (size <= 1024)
        // [128+1,1024] 16B -->4位
            return _Index(size - 128, 4) + group_array[0];
    if (size <= 8 * 1024)
        // [1024+1,8*1024] 128B -->7位
            return _Index(size - 1024, 7) + group_array[1] + group_array[0];
    if (size <= 64 * 1024)
        // [8*1024+1,64*1024] 1024B -->10位
            return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
    if (size <= 256 * 1024)
        // [64*1024+1,256*1024] 8 * 1024B  -->13位
            return _Index(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];

    assert(false);
    return -1;
}

// 获取不同块大小的内存块最高可申请内存块数的上限阈值
inline size_t BlockThreshHold(const size_t align_size) noexcept {
    assert(align_size > 0);

    size_t max_block_count = constant::MAX_ALLOC_BYTES / align_size;

    // 对于超小块内存， 不应该同时申请太多小块， 因此设置上限，控制在512块以内
    if (max_block_count > 512) max_block_count = 512;
    // 对于大块内存， 可能值会<2, 那么应该让它 = 2， 又不应该超过2， 不然浪费内存空间可能过大
    if (max_block_count < 2) max_block_count = 2;

    // 控制所有块的阈值位于 [2, 512]之间
    return max_block_count;
}

// Three-level radix tree for 64-bit address space
template <int BITS>
class TCMalloc_PageMap3 {
private:
    static const int ROOT_BITS = 8;  // 根节点：64位下，前8位表示根节点
    static const int ROOT_LENGTH = 1 << ROOT_BITS;  // 根节点的长度

    static const int MID_BITS = 8;  // 中间节点：64位下，中间8位表示中间节点
    static const int MID_LENGTH = 1 << MID_BITS;  // 中间节点的长度

    static const int LEAF_BITS = BITS - ROOT_BITS - MID_BITS;  // 剩余的BITS用于叶子节点
    static const int LEAF_LENGTH = 1 << LEAF_BITS;  // 叶子节点的长度

    // Leaf node
    struct Leaf {
        void* values[LEAF_LENGTH];
    };

    // 中间节点
    struct Mid {
        Leaf* leafs[MID_LENGTH];
    };

    // 根节点
    struct Root {
        Mid* mids[ROOT_LENGTH];
    };

    Root* root_;  // 整个树的根

public:
    typedef size_t Number;  // 64位系统下的地址类型

    explicit TCMalloc_PageMap3() {
        root_ = new Root();
        memset(root_, 0, sizeof(*root_));  // 初始化根节点
        PreallocateMoreMemory();  // 预分配内存，确保能存储2^64大小的数据
    }

    void* get(Number k) const {
        const Number i1 = k >> (MID_BITS + LEAF_BITS);  // 根节点索引
        const Number i2 = (k >> LEAF_BITS) & (MID_LENGTH - 1);  // 中间节点索引
        const Number i3 = k & (LEAF_LENGTH - 1);  // 叶子节点索引

        if ((k >> BITS) > 0 || root_->mids[i1] == NULL || root_->mids[i1]->leafs[i2] == NULL) {
            return NULL;  // 如果超出范围或者对应节点为空，返回NULL
        }
        return root_->mids[i1]->leafs[i2]->values[i3];  // 返回对应叶子节点的值
    }

    void set(Number k, void* v) {
        const Number i1 = k >> (MID_BITS + LEAF_BITS);  // 根节点索引
        const Number i2 = (k >> LEAF_BITS) & (MID_LENGTH - 1);  // 中间节点索引
        const Number i3 = k & (LEAF_LENGTH - 1);  // 叶子节点索引

        assert(i1 < ROOT_LENGTH);
        if (root_->mids[i1] == NULL) {
            root_->mids[i1] = new Mid();  // 如果没有中间节点，创建一个新的中间节点
            memset(root_->mids[i1], 0, sizeof(Mid));  // 初始化中间节点
        }
        if (root_->mids[i1]->leafs[i2] == NULL) {
            root_->mids[i1]->leafs[i2] = new Leaf();  // 如果没有叶子节点，创建一个新的叶子节点
            memset(root_->mids[i1]->leafs[i2], 0, sizeof(Leaf));  // 初始化叶子节点
        }

        root_->mids[i1]->leafs[i2]->values[i3] = v;  // 设置值
    }

    // 确保从start开始的n页内存空间已经被开辟
    bool Ensure(Number start, size_t n) {
        for (Number key = start; key <= start + n - 1;) {
            const Number i1 = key >> (MID_BITS + LEAF_BITS);  // 根节点索引
            const Number i2 = (key >> LEAF_BITS) & (MID_LENGTH - 1);  // 中间节点索引

            // 检查溢出
            if (i1 >= ROOT_LENGTH)
                return false;

            // 如果根节点没有中间节点，分配一个
            if (root_->mids[i1] == NULL) {
                root_->mids[i1] = new Mid();
                memset(root_->mids[i1], 0, sizeof(Mid));
            }

            // 如果中间节点没有叶子节点，分配一个
            if (root_->mids[i1]->leafs[i2] == NULL) {
                root_->mids[i1]->leafs[i2] = new Leaf();
                memset(root_->mids[i1]->leafs[i2], 0, sizeof(Leaf));
            }

            // 递增key，跳过当前叶子节点覆盖的区域
            key = ((key >> (MID_BITS + LEAF_BITS)) + 1) << (MID_BITS + LEAF_BITS);
        }
        return true;
    }

    // 预分配足够的内存空间
    void PreallocateMoreMemory() {
        // 分配足够的内存来跟踪所有可能的页面
        Ensure(0, 1 << BITS);
    }
};


}

}

