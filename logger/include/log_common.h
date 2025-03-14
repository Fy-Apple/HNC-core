#pragma once
#include <string>


namespace hnc::core::logger::details {
namespace constant {
// 写入日志的测试文件名
constexpr std::string LOG_FILE_NAME = "log/test_log";

constexpr size_t LOG_BUFFER_SIZE = 1024;  // 日志条目个数
constexpr size_t LOG_ENTRY_SIZE = 256; // 每条日志最大长度

}


}