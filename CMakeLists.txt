# 头文件
include_directories(logger)
include_directories(logger/include)
include_directories(memory_pool)
include_directories(memory_pool/include)
include_directories(thread_pool)
include_directories(thread_pool/include)

# 设置源文件
set(SOURCES
        thread_pool/src/task.cpp
        thread_pool/src/thread.cpp
        thread_pool/src/thread_pool.cpp


        logger/src/log_buffer.cpp
        logger/src/log_thread.cpp
        logger/src/logger.cpp

        memory_pool/src/freelist.cpp
        memory_pool/src/thread_cache.cpp
        memory_pool/src/central_cache.cpp
        memory_pool/src/page_cache.cpp
)

# 生成静态库
add_library(core STATIC ${SOURCES})

# 如果有外部依赖库的话（比如pthread）
target_link_libraries(core PUBLIC pthread)


add_subdirectory(logger/test)
add_subdirectory(memory_pool/test)
add_subdirectory(thread_pool/test)
