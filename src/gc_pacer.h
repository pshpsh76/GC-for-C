#pragma once
#include <chrono>
#include <cstddef>
#include <mutex>

const constexpr double kDefaultAlpha = 0.2, kDefaultPeak = 2;
const constexpr size_t kDefaultUpdateFreq = 20;

class GCPacer {
public:
    GCPacer(size_t threshold_bytes, size_t threshold_calls, double alpha = kDefaultAlpha,
            double peak_factor = kDefaultPeak, size_t update_frequency = kDefaultUpdateFreq);

    void Update(size_t allocated_bytes, size_t allocation_calls);
    bool ShouldTrigger();
    void Reset();
    size_t threshold_bytes_;
    size_t threshold_calls_;

private:
    double alpha_;
    double peak_factor_;
    size_t update_frequency_;

    double smoothed_rate_bytes_ = 0;
    double smoothed_rate_calls_ = 0;
    double instantaneous_rate_bytes_ = 0;
    double instantaneous_rate_calls_ = 0;

    size_t accumulation_count_ = 0;
    size_t accumulated_bytes_ = 0;
    size_t accumulated_calls_ = 0;

    size_t total_bytes_ = 0;
    size_t total_calls_ = 0;

    std::chrono::steady_clock::time_point last_update_time_;
    std::mutex sync_;
};
