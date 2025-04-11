#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <thread>
#include <chrono>
#include "gc_fwd.h"
#include "gc_pacer.h"

const constexpr size_t kDefaultThresholdBytes = 1024 * 1024, kDefaultThreasholdCalls = 1000;
const constexpr std::chrono::milliseconds kDefaultGCInterval =
    std::chrono::milliseconds(1000) * 60 * 2;  // 2 minutes

class GCScheduler {
public:
    GCScheduler(GCImpl* gc, size_t threshold_bytes = kDefaultThresholdBytes,
                size_t threshold_calls = kDefaultThreasholdCalls,
                std::chrono::milliseconds collection_interval = kDefaultGCInterval);
    ~GCScheduler();

    void UpdateAllocationStats(size_t size);
    void Start();
    void Stop();
    void Shutdown();

    void TriggerCollect();
    void WaitCollect();

    std::chrono::milliseconds GetCollectionInterval();
    void SetCollectionInterval(std::chrono::milliseconds collection_interval);

    size_t GetThresholdBytes();
    void SetThresholdBytes(size_t bytes);

    size_t GetThresholdCalls();
    void SetThresholdCalls(size_t calls);

    void ResetStats();

private:
    void SchedulerLoop();

    GCImpl* gc_;
    GCPacer pacer_;
    std::chrono::milliseconds collection_interval_;
    std::atomic<bool> stop_flag_, params_changed_, collect_triggered_ = false,
                                                   collect_done_ = false, shutdown_ = false;
    std::thread scheduler_thread_;
    std::mutex lock_scheduler_;
    std::mutex wait_mutex_;
    std::condition_variable loop_cv_, wait_collect_;
};
