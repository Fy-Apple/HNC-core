#pragma once

#include <mutex>

#include "hnc_log.h"

namespace hnc::core::mem_pool::details {
/** 双向链表节点类型Span,内部管理多个Page(OS Page,8KB) */
struct Span {
    size_t _page_id{0}; // 当前span管理的页起始序号
    size_t _page_size{0}; // 当前span管理的页面数量
    size_t _block_size{0}; // 当前span内的内存块大小

    void* _freelist_header{nullptr}; // span管理的块头节点
    size_t _use_count{0}; // 当前span分配出去的“内存块”大小，非page

    Span* _next{nullptr}; // 后继Span
    Span* _prev{nullptr}; // 前驱Span

    // false 表示Span默认在pc中
    bool _is_use{false};
};

/** span双向链表 */
class SpanList {
public:
    SpanList() {
        // 初始化头节点
        /**
         * 此处 三个单例还没有初始化！  尝试调用会错误， 所以头节点不能是指针
         */
        // _m_header = new Span();
        _m_header =  &_m_header_span;
        _m_header->_prev = _m_header->_next = _m_header;
    }
    ~SpanList() {
        /** 注意， 其他span不需要删除，全部由pc管理，只删除自己申请的header头节点 */
        //delete _m_header;
    }
    class Iterator {
    public:
        explicit Iterator(Span* span) : _m_span(span) {}
        // 重载解引用运算符
        Span* operator*() const {
            return _m_span;
        }

        // 重载箭头运算符
        Span* operator->() const {
            return _m_span;
        }

        // 前置++运算符, 后置++运算符就是用一个tmp复制一下
        Iterator& operator++() {
            _m_span = _m_span->_next;
            return *this;
        }

        // 比较迭代器是否相等
        bool operator!=(const Iterator& other) const {
            return _m_span != other._m_span;
        }
    private:
        Span *_m_span;
    };
    // 双向链表的第一个节点
    Iterator begin() const noexcept{
        return Iterator (_m_header->_next);
    }
    // 双链表的头节点即是尾节点
    Iterator  end() const noexcept {
        return Iterator (_m_header);
    }
    // 判空
    bool empty() const noexcept {
        return _m_header == _m_header->_next;
    }
    // 插入一个Span节点在pos的前面
    void insert(Span* pos, Span* span) const noexcept {
        assert(pos != nullptr);
        assert(span);

        pos->_prev->_next = span;
        span->_prev = pos->_prev;
        span->_next = pos;
        pos->_prev = span;
        logger::log_trace("insert span, page_size=" + std::to_string(span->_page_size));
    }
    // 删除一个Span节点
    void erase(const Span* span) const noexcept {
        assert(span != nullptr);
        assert(span != _m_header);

        span->_prev->_next = span->_next;
        span->_next->_prev = span->_prev;
        // pos指向的span节点不需要删除， 而是进行回收, 由pc统一回收

        logger::log_trace("erase span, page_size=" + std::to_string(span->_page_size));
    }

    Span* pop_front() const noexcept {
        assert(!empty());
        Span* front = _m_header->_next;
        erase(front);
        return front;
    }

    void push_front(Span *span) const noexcept {
        span->_prev = _m_header;
        span->_next = _m_header->_next;
        _m_header->_next->_prev = span;
        _m_header->_next = span;
    }
    void lock() noexcept {
        _m_mtx.lock();
    }
    void unlock() noexcept {
        _m_mtx.unlock();
    }
private:
    Span* _m_header;
    Span _m_header_span;
    std::mutex _m_mtx;
};

}