#pragma once

#include "log_common.h"

#include <atomic>
#include <fstream>

namespace hnc::core::logger::details{
/**
 * @brief 日志缓冲区类，存储日志并支持多线程安全写入
 *
 * 该类提供一个 **环形缓冲区**，生产者线程并发写入日志，消费者线程读取日志。
 * - 采用 `atomic` 进行无锁索引管理
 * - 支持 `std::shared_mutex` 读写锁
 * - 满了后需要外部进行缓冲区交换
 */
class LogBuffer{
public:
    LogBuffer();
    ~LogBuffer();

    LogBuffer(const LogBuffer&) = delete;
    LogBuffer(LogBuffer &&) = delete;

    LogBuffer& operator=(const LogBuffer&) = delete;
    LogBuffer& operator=(LogBuffer &&) = delete;

    /**
     * @brief 获取一个 唯一的 日志缓存数组下标
     *
     * @return 缓存满了则 返回false
     */
    bool get_idx(int &index) noexcept ;

    /**
     * @brief 添加日志到缓冲区 中的 idx下标处
     * @param msg 日志字符串
     * @param len 日志长度
     * @param idx 绝对不会越界的缓存唯一下标
     */
    void add_log(const char* msg, u_int8_t len, int idx) noexcept;

    /**
     * @brief 判断缓冲区是否已经恢复， 即可写的位置 为 0
     */
    bool empty() const noexcept;

    /**
     * @brief 将缓冲区数据写入 指定的out file stream文件流
     */
    void flush(std::ofstream& logfile, bool is_all) noexcept;
private:

    /**
     * @brief 复位缓冲区
     * 此函数只会被后台日志线程调用, 因此不需要任何加锁机制, 锁由外部控制
     */
    void m_reset() noexcept;


    char m_buffer_[constant::LOG_BUFFER_SIZE][constant::LOG_ENTRY_SIZE]{};  // 1024 个日志，每个 256B
    std::atomic<size_t> m_index_;  // 当前日志写入的位置
};

// 全局的三块缓冲区
inline LogBuffer buffers[3];

}