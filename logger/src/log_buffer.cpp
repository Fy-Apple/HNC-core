#include "log_buffer.h"

#include <cstring>

namespace hnc::core::logger::details {

LogBuffer::LogBuffer() : m_index_(0) {

}

LogBuffer::~LogBuffer() {

}

/**
 * @brief 获取一个 唯一的 日志缓存数组下标
 *
 * @return 缓存满了则 返回false
 */
bool LogBuffer::get_idx(int &index) noexcept {
    index = m_index_.fetch_add(1, std::memory_order_relaxed);
    // TODO : 这里index 是 fetch_add， 如果越界了 注意回退, -> 已解决
    if (index >= constant::LOG_BUFFER_SIZE)
        return false;
    return true;
}

/**
 * @brief 添加日志到缓冲区 中的 idx下标处
 * @param msg 日志字符串
 * @param len 日志长度
 * @param idx 绝对不会越界的缓存唯一下标
 */
void LogBuffer::add_log(const char *msg, const u_int8_t len, const int idx) noexcept {
    // TODO : 是否还有其他可以写入的函数
    // 第一个字节写入日志长度，方便后续 后台日志线程 write()
    m_buffer_[idx][0] = len;
    std::memcpy(m_buffer_[idx] + 1, msg, len);
}


/**
 * @brief 判断缓冲区是否已经恢复， 即可写的位置 为 0
 */
bool LogBuffer::empty() const noexcept {
    return m_index_.load(std::memory_order_relaxed) == 0;
}

/**
 * @brief 将缓冲区数据写入 指定的out file stream文件流
 */
void LogBuffer::flush(std::ofstream &logfile, const bool is_all) noexcept {
    const int buffer_size = is_all ? constant::LOG_BUFFER_SIZE : std::min(m_index_.load(std::memory_order_acquire), constant::LOG_BUFFER_SIZE);
    for (int i = 0; i < buffer_size; ++i) {
        const uint8_t log_len = static_cast<uint8_t>(m_buffer_[i][0]);  // 读取日志有效字节数
        m_buffer_[i][log_len + 1] = '\n'; // 追加换行符
        logfile.write(m_buffer_[i] + 1, log_len + 1);  // 写入有效数据（跳过第长度一个字节）
    }
    logfile.flush();
    // 重置该缓冲区状态
    m_reset();
}

/**
 * @brief 复位缓冲区
 * 此函数只会被后台日志线程调用, 因此不需要任何加锁机制, 锁由外部控制
 */
void LogBuffer::m_reset() noexcept {
    m_index_.store(0, std::memory_order_relaxed);
    std::memset(m_buffer_, 0, sizeof(m_buffer_));  // 清空所有内存
}
}








