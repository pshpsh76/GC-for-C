#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include "gc_fwd.h"

class GCScheduler {
public:
    GCScheduler(GCImpl* gc, size_t threshold_bytes = 1024 * 1024, size_t threshold_calls = 1000);
    ~GCScheduler();

    void UpdateAllocationStats(size_t size);

    void Start();
    void Stop();

private:
    void SchedulerLoop();
    void ResetStats();

    GCImpl* gc_;
    size_t allocated_bytes_since_last_gc_;
    size_t allocation_calls_since_last_gc_;
    size_t threshold_bytes_;
    size_t threshold_calls_;
    std::atomic<bool> stop_flag_;
    std::thread scheduler_thread_;
};
