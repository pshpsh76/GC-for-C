#include "gc_pacer.h"
#include <mutex>

GCPacer::GCPacer(size_t threshold_bytes, size_t threshold_calls, double alpha, double peak_factor,
                 size_t update_frequency)
    : threshold_bytes_(threshold_bytes),
      threshold_calls_(threshold_calls),
      alpha_(alpha),
      peak_factor_(peak_factor),
      update_frequency_(update_frequency),
      last_update_time_(std::chrono::steady_clock::now()) {
}

void GCPacer::Update(size_t allocated_bytes, size_t allocation_calls) {
    std::lock_guard<std::mutex> lock(sync_);
    total_bytes_ += allocated_bytes;
    accumulated_bytes_ += allocated_bytes;

    total_calls_ += allocation_calls;
    accumulated_calls_ += allocation_calls;
    ++accumulation_count_;

    if (accumulation_count_ < update_frequency_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count();

    static const double kMult = 1000.0;

    instantaneous_rate_bytes_ = accumulated_bytes_ * kMult / elapsed_ms;
    instantaneous_rate_calls_ = accumulated_calls_ * kMult / elapsed_ms;

    smoothed_rate_bytes_ = alpha_ * instantaneous_rate_bytes_ + (1 - alpha_) * smoothed_rate_bytes_;
    smoothed_rate_calls_ = alpha_ * instantaneous_rate_calls_ + (1 - alpha_) * smoothed_rate_calls_;

    last_update_time_ = now;
    accumulation_count_ = 0;
    accumulated_bytes_ = 0;
    accumulated_calls_ = 0;
}

bool GCPacer::ShouldTrigger() {
    std::lock_guard<std::mutex> lock(sync_);
    double ratio_bytes = static_cast<double>(total_bytes_) / threshold_bytes_;
    double ratio_calls = static_cast<double>(total_calls_) / threshold_calls_;
    double base_trigger_ratio = std::max(ratio_bytes, ratio_calls);

    bool regular_trigger = base_trigger_ratio >= 1.0;
    bool peak_trigger = (instantaneous_rate_bytes_ > peak_factor_ * smoothed_rate_bytes_) ||
                        (instantaneous_rate_calls_ > peak_factor_ * smoothed_rate_calls_);
    return regular_trigger || peak_trigger;
}

void GCPacer::Reset() {
    std::lock_guard<std::mutex> lock(sync_);
    smoothed_rate_bytes_ = 0.0;
    smoothed_rate_calls_ = 0.0;
    instantaneous_rate_bytes_ = 0.0;
    instantaneous_rate_calls_ = 0.0;
    accumulation_count_ = 0;
    accumulated_bytes_ = 0;
    accumulated_calls_ = 0;
    total_bytes_ = 0;
    total_calls_ = 0;
    last_update_time_ = std::chrono::steady_clock::now();
}