#include <cstddef>
#include <gtest/gtest.h>
#include "gc.h"
#include "utils.h"
#include <thread>
#include <chrono>

TEST(GCAutoTest, BytesThreshold) {
    gc_init(nullptr, 0);
    gc_enable_auto();
    gc_reset_info();
    ResetCounter();
    size_t threshold = 100;
    gc_set_bytes_threshold(threshold);
    gc_malloc(threshold, CounterFinalizer);
    gc_malloc(threshold, CounterFinalizer);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_GE(GetCounter(), 1);  // auto gc collect can not collect last allocation

    gc_malloc(threshold / 2, CounterFinalizer);
    ASSERT_GE(GetCounter(), 1);
    gc_collect();
    gc_free_all();
}

TEST(GCAutoTest, CallThreshold) {
    gc_init(nullptr, 0);
    gc_reset_info();
    ResetCounter();
    size_t call_threshold = 3;
    gc_set_bytes_threshold(100000);
    gc_set_calls_threshold(call_threshold);

    gc_malloc(1, CounterFinalizer);
    ASSERT_EQ(GetCounter(), 0);

    gc_malloc(1, CounterFinalizer);
    ASSERT_EQ(GetCounter(), 0);

    gc_malloc(1, CounterFinalizer);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_EQ(GetCounter(), 3);
    gc_free_all();
}

TEST(GCAutoTest, TimeInterval) {
    gc_init(nullptr, 0);
    gc_reset_info();
    ResetCounter();
    gc_set_collect_interval(10);

    gc_malloc(1, CounterFinalizer);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(GetCounter(), 1);
}

TEST(GCAutoTest, Peak) {
    gc_set_collect_interval(1000 * 60 * 2);
    gc_reset_info();
    ResetCounter();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    gc_set_bytes_threshold(1000000000);
    gc_set_calls_threshold(1000000000);

    for (int i = 0; i < 100000; ++i) {
        if (i < 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        gc_malloc(1, CounterFinalizer);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_GE(GetCounter(), 1);
}
