#pragma once

#include "common.h"
#include <freelist.h>

#ifdef _WIN32
#include<Windows.h>
#else
#include <unistd.h>
#include <stdio.h>
#endif

namespace hnc::core::mem_pool::details {
inline void* SystemAlloc(const size_t page_count)
{
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

// 内存池， 对象size固定
template<class T>
class FixedMemPool {
public:
    T* New() {
        // ① 自由链表不为nullptr，即有内存块，直接分配出去, 优先使用归还的内存块分配
        if (_m_free_list){
            // 获取下一个内存块地址
            void *next = GetNextAddr(_m_free_list);

            auto p = _m_free_list;
            // 更新自由链表
            _m_free_list = next;
            // 显式对第一个节点内存块调用T构造方法
            new(p)T;
            return static_cast<T*>(p);
        }
        // ② 自由链表没有内存块
        // 内存区域中剩余字节数少于 T 或者 初次开辟内存
        if (_m_remain_bytes < sizeof(T))
        {
            // 更新剩余字节数 ，申请128KB
            _m_remain_bytes = 128 * 1024;

            // 使用brk()系统调用调整进程堆区指针来获得堆内存
            _m_mem = static_cast<char*>(SystemAlloc(_m_remain_bytes >> constant::PAGE_SHIFT));
        }

        // 初始化内存块上的T对象
        auto obj = reinterpret_cast<T*>(_m_mem);
        // 判断一下T的大小，小于指针就给一个指针大小，大于指针就还是T的大小
        const size_t obj_size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
        // 更新内存指针和剩余字节数
        _m_mem += obj_size;
        _m_remain_bytes -= obj_size;
        // 显式调用对象构造函数
        new(obj)T;
        return obj;
    }

    // 回收小内存块
    void Delete(T* obj)
    {
        // 先显式调用对象的析构函数
        obj->~T();

        // 将该内存块回收
        GetNextAddr(obj) = _m_free_list; // obj内存块存储原头节点地址
        _m_free_list = obj; // 自由链表执行新内存块地址
    }

    void lock() noexcept {
        _m_mtx.lock();
    }

    void unlock() noexcept {
        _m_mtx.unlock();
    }

private:
    char* _m_mem{nullptr}; // 指向内存块的指针
    size_t _m_remain_bytes{0}; // 剩余字节数
    void* _m_free_list{nullptr}; // 自由链表

    std::mutex _m_mtx;
};
}