add_executable(gc_test
    gc_test.cpp
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
