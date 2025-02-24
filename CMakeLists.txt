# 设置源文件
set(SOURCES
        memory_pool/test/main.cpp
        memory_pool/src/freelist.cpp
        memory_pool/src/thread_cache.cpp
        memory_pool/src/central_cache.cpp
        memory_pool/src/page_cache.cpp
)

# 生成静态库
add_library(core STATIC ${SOURCES})

# 如果有外部依赖库的话（比如pthread）
target_link_libraries(core PUBLIC pthread)

# 如果有头文件
target_include_directories(core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/memory_pool/include
)

add_subdirectory(memory_pool/test)
