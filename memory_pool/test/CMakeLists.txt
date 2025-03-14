set(TEST_POOL_SOURCES
        test_pool.cpp
)

add_executable(mp_test ${TEST_POOL_SOURCES})

target_link_libraries(mp_test PUBLIC core pthread)

set(BENCHMARK_SOURCES
        benchmark.cpp
)

add_executable(mp_benchmark ${BENCHMARK_SOURCES})

target_link_libraries(mp_benchmark PUBLIC core pthread)
