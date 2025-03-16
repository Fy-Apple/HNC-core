#pragma once
#include <assert.h>
#include <ctime>
#include <format>
#include <source_location>

#include "logger.h"

/**
* 全局 Logger 接口
*/

namespace hnc::core::logger {
// 定义遍历日志等级宏， 后续修改日志等级只需要修改这个宏即可
#define _FOREACH_LOG_LEVEL(f) \
f(trace) \
f(debug) \
f(info) \
f(critical) \
f(warn) \
f(error) \
f(fatal)

// 日志等级
enum class Level : std::uint8_t {
#define _FUNCTION(name) name,
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION
};

namespace details {
/**
 * @brief 获取日志等级对应的字符串
 * @return 格式化后的时间字符串 [YYYY-MM-DD HH:MM:SS]
 */
inline std::string log_level_str(const Level level) {
    switch (level) {
#define _FUNCTION(name) case Level::name: return #name;
        _FOREACH_LOG_LEVEL(_FUNCTION)
    #undef _FUNCTION
        }
    assert(false);
    return "unknown";
}

/**
 * @brief 反射， 将字符串映射到 日志 等级
 * @return 日志等级level
 */
inline Level log_level(const std::string &lev) {
#define _FUNCTION(name) if(lev == #name) return Level::name;
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION
  return Level::info;
}
}

// 读取环境变量中设置好的日志等级， 若没有则默认设置为 INFO

inline Level GLOBAL_LOG_LEVEL = [] () -> Level {
    // C++17特性 ↓ 获取环境变量
    if (const auto lev = std::getenv("HNC_LOG_LEVEL")) {
        return details::log_level(lev);
    }
    return Level::debug;
} ();


namespace details{
/**
 * @brief 统一的日志封装函数
 * @param level 日志等级
 * @param msg 日志内容
 * @param loc 代码所在位置
 */
inline void log_message(const Level level, const std::string& msg, const std::source_location loc) noexcept {

    // 若日志等级不足则直接跳过 不写入
    if (level < GLOBAL_LOG_LEVEL) {
        return;
    }

    // time_t : 从 纪元（Epoch） 开始到当前时间的秒数。  1970年1月1日 00:00:00 UTC
    const std::time_t time_now = std::time(nullptr); // 获取当前时间的 秒数
    std::tm tm_now;
    localtime_r(&time_now, &tm_now); // 线程安全的 转换为 时间结构体(年月日时分秒)

    char time_buffer[20]; // 写入栈上缓存 刚刚好是19个字节 + '\0' = 20 Byte
    std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &tm_now); // [2000-07-03 00:00:00]

    // TODO : 使用 fmt库的 format 替代 C++20 的 std::format 以支持 Args && args ...
    // const std::string log_msg = std::format("[{}] [{}] [{}:{}:{}] {}",
    //                                   log_level_str(level), time_buffer,
    //                                   loc.file_name(), loc.line(), loc.function_name(),
    //                                   msg);
    const std::string log_msg = std::format("[{}] [{}] [{}] {}",
                                      log_level_str(level), time_buffer,
                                      loc.function_name(),
                                      msg);
    // 调用全局单例 写入 日志
    Logger::instance().log(log_msg);
}
}



// 提供不同日志等级的 api 接口
#define _FUNCTION(name) inline void log_##name(const std::string& msg, std::source_location loc = std::source_location::current()) noexcept { details::log_message(Level::name, msg, loc); }
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION

#undef _FOREACH_LOG_LEVEL
}