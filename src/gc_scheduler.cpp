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
    Start();
}

GCScheduler::~GCScheduler() {
    Shutdown();
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void GCScheduler::Start() {
    stop_flag_ = false;
    if (scheduler_thread_.joinable()) {
        return;
    }
    scheduler_thread_ = std::thread(&GCScheduler::SchedulerLoop, this);
}

void GCScheduler::Stop() {
    stop_flag_ = true;
    loop_cv_.notify_one();
}

void GCScheduler::Shutdown() {
    shutdown_ = true;
    loop_cv_.notify_one();
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void GCScheduler::TriggerCollect() {
    collect_triggered_ = true;
    loop_cv_.notify_one();
}

void GCScheduler::WaitCollect() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_collect_.wait(lock, [this] { return collect_done_.load(); });
    collect_done_ = false;
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
    while (!shutdown_) {

        std::unique_lock<std::mutex> lock(lock_scheduler_);
        params_changed_ = false;
        wait_collect_.notify_one();
        bool notified = loop_cv_.wait_for(lock, collection_interval_, [this]() {
            return stop_flag_.load() || params_changed_.load() || collect_triggered_.load() ||
                   shutdown_.load() || pacer_.ShouldTrigger();
        });
        lock.unlock();
        if ((!stop_flag_ && (pacer_.ShouldTrigger() || !notified)) || collect_triggered_.load()) {
            collect_done_ = false;
            gc_->Collect();
            collect_done_ = true;
            wait_collect_.notify_one();
            ResetStats();
        }
    }
}

void GCScheduler::ResetStats() {
    collect_triggered_ = false;
    pacer_.Reset();
}
