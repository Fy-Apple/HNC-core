set(TEST_LOGGER_SOURCES
        test_logger.cpp
)


add_executable(log_test ${TEST_LOGGER_SOURCES})

target_link_libraries(log_test PUBLIC hnc_core)

set(BENCHMARK_SOURCES
        benchmark.cpp
)


add_executable(log_benchmark ${BENCHMARK_SOURCES})

target_link_libraries(log_benchmark PUBLIC hnc_core)
