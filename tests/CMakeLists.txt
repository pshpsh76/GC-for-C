add_executable(gc_test
    gc_lib_test.cpp gc_sched_test.cpp
)

target_link_libraries(gc_test PRIVATE
    gtest
    gtest_main
    garbage_collector
)


target_include_directories(gc_test PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME GCTest COMMAND gc_test)

add_executable(gc_benchmark gc_lib_bench.cpp gc_sched_bench.cpp)

target_link_libraries(gc_benchmark PRIVATE
    benchmark::benchmark
    garbage_collector
)

target_include_directories(gc_benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_test(NAME GCBenchmark COMMAND gc_benchmark)