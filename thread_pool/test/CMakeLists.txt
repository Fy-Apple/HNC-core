
# 设置源文件
set(SOURCES
        tp_test.cpp
)

# 生成静态库
add_executable(tp_test ${SOURCES})

# 如果有外部依赖库的话
target_link_libraries(tp_test PUBLIC core)
