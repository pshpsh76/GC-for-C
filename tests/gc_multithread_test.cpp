#include <chrono>
#include <cstddef>
#include <thread>
#include <gtest/gtest.h>
#include "gc.h"
#include "utils.h"

const int kThreads = 8;

TEST(GarbageCollectorTest, MultithreadedAllocation) {
    gc_disable_auto();
    gc_init(nullptr, 0);
    ResetCounter();
    const int k_alloc_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([]() {
            for (int i = 0; i < k_alloc_per_thread; ++i) {
                void* ptr = gc_malloc(64, CounterFinalizer);
                ASSERT_NE(ptr, nullptr);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
    gc_collect_blocked();
    wait_bit();
    ASSERT_EQ(GetCounter(), kThreads * k_alloc_per_thread);
}

TEST(GarbageCollectorTest, MultithreadedStackRoots) {
    gc_disable_auto();
    ResetCounter();

    const int per_thread = 100;

    std::vector<std::thread> threads;
    std::vector<int*> all_pointers;
    std::mutex mutex;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            int* local_roots[per_thread];
            GCRoot root = {reinterpret_cast<void*>(local_roots), sizeof(local_roots)};
            gc_add_root(root);

            for (int i = 0; i < per_thread; ++i) {
                local_roots[i] = static_cast<int*>(gc_malloc(sizeof(int), CounterFinalizer));
                *local_roots[i] = t * 1000 + i;
            }

            {
                std::lock_guard<std::mutex> lock(mutex);
                all_pointers.push_back(local_roots[0]);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            gc_collect();
            gc_delete_root(root);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int* ptr : all_pointers) {
        ASSERT_TRUE(ptr != nullptr);
    }

    gc_collect_blocked();
    wait_bit();
    ASSERT_EQ(GetCounter(), kThreads * per_thread);
}

TEST(GarbageCollectorTest, ParallelCollectDuringAlloc) {
    gc_disable_auto();
    gc_init(nullptr, 0);
    ResetCounter();

    const int k_iterations = 500;

    std::atomic<bool> running = true;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            gc_register_thread();
            gc_collect();
            for (int i = 0; i < k_iterations; ++i) {
                void* ptr = gc_malloc(1, CounterFinalizer);
                ASSERT_NE(ptr, nullptr);
            }
            gc_deregister_thread();
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    running = false;
    gc_disable_auto();
    gc_collect_blocked();
    wait_bit();
    ASSERT_GE(GetCounter(), kThreads * k_iterations);
}