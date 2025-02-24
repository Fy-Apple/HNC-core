
# 设置源文件
set(SOURCES
        main.cpp
)

# 生成静态库
add_executable(mp_test ${SOURCES})

# 如果有外部依赖库的话（比如pthread）
target_link_libraries(mp_test PUBLIC core)
