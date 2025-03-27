#include "gc_scheduler.h"
#include "gc_impl.h"

GCScheduler::GCScheduler(GCImpl* gc, size_t threshold_bytes, size_t threshold_calls)
    : gc_(gc),
      allocated_bytes_since_last_gc_(0),
      allocation_calls_since_last_gc_(0),
      threshold_bytes_(threshold_bytes),
      threshold_calls_(threshold_calls),
      
      stop_flag_(false) {
}

GCScheduler::~GCScheduler() {
    Stop();
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void GCScheduler::Start() {
    scheduler_thread_ = std::thread(&GCScheduler::SchedulerLoop, this);
}

void GCScheduler::Stop() {
    stop_flag_ = true;
}

void GCScheduler::UpdateAllocationStats(size_t size) {
    allocated_bytes_since_last_gc_ += size;
    ++allocation_calls_since_last_gc_;

    if (allocated_bytes_since_last_gc_ >= threshold_bytes_ ||
        allocation_calls_since_last_gc_ >= threshold_calls_) {
        gc_->Collect();
        ResetStats();
    }
}

void GCScheduler::SchedulerLoop() {
    while (!stop_flag_) {
        std::this_thread::sleep_for(std::chrono::minutes(2));
        gc_->Collect();
        ResetStats();
    }
}

void GCScheduler::ResetStats() {
    allocated_bytes_since_last_gc_ = 0;
    allocation_calls_since_last_gc_ = 0;
}
