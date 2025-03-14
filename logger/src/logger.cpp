#include "logger.h"
#include "log_buffer.h"

#include <string>
#include <mutex>

namespace hnc::core::logger::details {


Logger::Logger() {
    m_log_thread_.start();
}

Logger::~Logger() {}

/**
 * @brief 多生产者写入日志 ， 主从缓冲区切换
 * @param msg 封装好的日志内容
 * @param loc 自动生成日志附加消息
 */
void Logger::log(const std::string& msg) const noexcept{
    while (true) {
        // 1. 先获取共享锁，
        {
            std::shared_lock<std::shared_mutex> shared_lock(m_meta_mtx_);
            if (int idx; m_log_thread_.primary_buffer()->get_idx(idx)) {
                // 从主缓冲区获得了可写入的缓冲区下标，
                m_log_thread_.primary_buffer()->add_log(msg.c_str(), msg.size(), idx);
                return; // 写入成功，直接返回
            }
            // 若缓冲区已满， 则释放读锁
        }
        // 2. 主缓冲区已满， 多线程竞争 独占写锁，  得到独占写锁的生产者进行主从缓冲区切换
        std::unique_lock<std::shared_mutex> write_lock(m_meta_mtx_);
        // 双重判断 缓冲区是否已满， 因为可能有多个生产者阻塞在 写锁上， 若已经完成缓冲区切换， 则 continue 即可， 逻辑会执行进入 上方作用域
        if (m_log_thread_.primary_buffer()->empty()) continue; // 这里会释放 独占锁

        // 真正进行切换缓冲区的线程, 交换主从缓冲区
        m_log_thread_.primary_swap();

        // 3. 此时主缓冲区可以继续写入日志，但是先不释放锁， 从缓冲区是满的， 先通知 日志线程，交换从备缓冲区 并write
        m_log_thread_.notify();
        // 阻塞等待后台日志线程交换完从备缓冲区, 所有生产者线程可恢复正常运行
        m_log_thread_.wait();

        // 在这里释放写锁后， 所有生产者就醒来了，可以继续写入日志缓冲区, while 逻辑会执行进入 最上方作用域
    }
}
}
