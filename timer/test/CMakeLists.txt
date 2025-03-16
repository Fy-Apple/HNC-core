
# 设置源文件
set(SOURCES
        time_test.cpp
)

# 生成静态库
add_executable(time_test ${SOURCES})

# 如果有外部依赖库的话
target_link_libraries(time_test PUBLIC core)
