#include <chrono>
#include <cstddef>
#include <thread>
#include <gtest/gtest.h>
#include "gc.h"
#include "utils.h"

const int kThreads = 8;
const int kIterations = 500;

TEST(MultiThreadGCTest, MultithreadedAllocation) {
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
    wait_bit();  // need wait for atomic operations finishing
    ASSERT_EQ(GetCounter(), kThreads * k_alloc_per_thread);
}

TEST(MultiThreadGCTest, MultithreadedStackRoots) {
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

TEST(MultiThreadGCTest, ParallelCollectDuringAlloc) {
    gc_disable_auto();
    gc_init(nullptr, 0);
    ResetCounter();

    std::atomic<bool> running = true;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([]() {
            gc_register_thread();
            gc_collect();
            for (int i = 0; i < kIterations; ++i) {
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
    ASSERT_GE(GetCounter(), kThreads * kIterations);
}

TEST(MultiThreadGCTest, AutoCollectWithThreads) {
    gc_enable_auto();
    gc_reset_info();
    ResetCounter();
    gc_set_bytes_threshold(1000);
    gc_set_calls_threshold(1000);
    gc_set_collect_interval(10);

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([]() {
            gc_register_thread();
            for (int i = 0; i < kIterations; ++i) {
                gc_malloc(32, CounterFinalizer);
            }
            gc_deregister_thread();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_GE(GetCounter(), kThreads * kIterations);
}

TEST(MultiThreadGCTest, ConcurrentEnableDisableScheduler) {
    gc_init(nullptr, 0);
    ResetCounter();

    std::atomic<bool> done = false;
    std::thread toggler([&done]() {
        for (int i = 0; i < 50; ++i) {
            gc_enable_auto();
            gc_disable_auto();
        }
        done = true;
    });

    std::thread allocator([&done]() {
        gc_register_thread();
        while (!done) {
            void* ptr = gc_malloc(64, CounterFinalizer);
            ASSERT_NE(ptr, nullptr);
        }
        gc_deregister_thread();
    });

    toggler.join();
    allocator.join();

    gc_collect_blocked();
    wait_bit();
    ASSERT_GE(GetCounter(), 0);  // sanity check; no crash expected
}

TEST(MultiThreadGCTest, DeregisterDuringCollect) {
    gc_disable_auto();
    gc_init(nullptr, 0);
    ResetCounter();

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([]() {
            gc_register_thread();
            gc_malloc(64, CounterFinalizer);
            gc_collect();  // Trigger collection before deregistration
            gc_deregister_thread();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    gc_collect_blocked();
    wait_bit();
    ASSERT_EQ(GetCounter(), kThreads);
}

TEST(MultiThreadGCTest, StaggeredRootLifetimes) {
    gc_disable_auto();
    ResetCounter();

    int* shared_ptr = nullptr;

    std::thread long_lived([&shared_ptr]() {
        int* local[1];
        local[0] = static_cast<int*>(gc_malloc(sizeof(int), CounterFinalizer));
        *local[0] = 123;
        shared_ptr = local[0];
        GCRoot root = {reinterpret_cast<void*>(local), sizeof(local)};
        gc_add_root(root);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        gc_delete_root(root);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::thread short_lived([]() {
        int* temp[1];
        temp[0] = static_cast<int*>(gc_malloc(sizeof(int), CounterFinalizer));
        GCRoot root = {reinterpret_cast<void*>(temp), sizeof(temp)};
        gc_add_root(root);
        gc_collect();
        gc_delete_root(root);
    });

    long_lived.join();
    short_lived.join();

    gc_collect_blocked();
    wait_bit();
    ASSERT_TRUE(shared_ptr != nullptr);
    ASSERT_EQ(GetCounter(), 2);
}
