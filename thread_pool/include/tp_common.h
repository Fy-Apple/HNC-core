#pragma once

#include <atomic>


namespace hnc::core::tp::details {
enum class TPoolMode {
    FIXED,
    CACHED,
};

namespace constant {
constexpr size_t INIT_THREAD_SIZE = 4;

}

}