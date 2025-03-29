#include "gc_scheduler.h"
#include <mutex>
#include "gc_impl.h"
#include "gc_pacer.h"

GCScheduler::GCScheduler(GCImpl* gc, size_t threshold_bytes, size_t threshold_calls,
                         std::chrono::milliseconds collection_interval)
    : gc_(gc),
      pacer_(threshold_bytes, threshold_calls),
      collection_interval_(collection_interval),
      stop_flag_(false) {
}

GCScheduler::~GCScheduler() {
    Stop();
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void GCScheduler::Start() {
    std::unique_lock<std::mutex> lock(lock_scheduler_);
    if (scheduler_thread_.joinable()) {
        stop_flag_ = true;
        lock.unlock();
        loop_cv_.notify_one();
        scheduler_thread_.join();
        lock.lock();
    }
    stop_flag_ = false;
    scheduler_thread_ = std::thread(&GCScheduler::SchedulerLoop, this);
}

void GCScheduler::Stop() {
    {
        std::lock_guard<std::mutex> lock(lock_scheduler_);
        stop_flag_ = true;
    }
    loop_cv_.notify_one();
}

std::chrono::milliseconds GCScheduler::GetCollectionInterval() {
    return collection_interval_;
}

void GCScheduler::SetCollectionInterval(std::chrono::milliseconds collection_interval) {
    {
        std::lock_guard<std::mutex> lock(lock_scheduler_);
        collection_interval_ = collection_interval;
    }
    params_changed_ = true;
    loop_cv_.notify_one();
}

size_t GCScheduler::GetThresholdBytes() {
    return pacer_.threshold_bytes_;
}

void GCScheduler::SetThresholdBytes(size_t bytes) {
    pacer_.SetThresholdBytes(bytes);
}

size_t GCScheduler::GetThresholdCalls() {
    return pacer_.threshold_calls_;
}

void GCScheduler::SetThresholdCalls(size_t calls) {
    pacer_.SetThresholdCalls(calls);
}

void GCScheduler::UpdateAllocationStats(size_t size) {
    pacer_.Update(size, 1);
    if (pacer_.ShouldTrigger()) {
        loop_cv_.notify_one();
    }
}

void GCScheduler::SchedulerLoop() {
    std::unique_lock<std::mutex> lock(lock_scheduler_);
    while (!stop_flag_.load()) {
        params_changed_ = false;
        loop_cv_.wait_for(lock, collection_interval_, [this]() {
            return stop_flag_.load() || pacer_.ShouldTrigger() || params_changed_.load();
        });
        if (stop_flag_.load()) {
            break;
        }
        gc_->Collect();
        ResetStats();
    }
}

void GCScheduler::ResetStats() {
    pacer_.Reset();
}
