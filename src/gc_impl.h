#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <vector>
#include "gc_fwd.h"
#include "gc.h"
#include "gc_scheduler.h"

struct Allocation {
    uintptr_t ptr;
    size_t size;
    FinalizerT finalizer;
    size_t last_valid_time;
};

constexpr int kAlignment = alignof(void**);
constexpr int kSize = sizeof(void**);

uintptr_t Aligned(uintptr_t ptr);

class GCImpl {
public:
    GCImpl();
    ~GCImpl();

    // Roots stuff
    void Init(const std::vector<GCRoot>& roots);
    void AddRoot(const GCRoot& root);
    void DeleteRoot(const GCRoot& root);

    // Memory allocation functions
    void* Malloc(size_t size, FinalizerT finalizer);
    void* Calloc(size_t nmemb, size_t size, FinalizerT finalizer);
    void* Realloc(void* ptr, size_t size, FinalizerT finalizer);
    void Free(uintptr_t ptr);
    void FreeAll();

    // Automatic memory management
    GCScheduler& GetScheduler();
    const GCScheduler& GetScheduler() const;
    void DisableScheduler();
    void EnableScheduler();

    // Thread safety
    void Safepoint();
    void RegisterThread();
    void DeregisterThread();

    // Collect
    void Collect();

private:
    // Allocations helpers
    void CreateAllocation(uintptr_t ptr, size_t size, FinalizerT finalizer);
    void DeleteAllocation(uintptr_t ptr);
    bool IsValidAllocation(const Allocation& alloc);
    void SortAllocations();

    // Mark Sweep part
    void StopWorld();
    void ResumeWorld();
    void CollectPrepare();
    std::vector<Allocation*> MarkRoots();
    void MarkHeapAllocs(const std::vector<Allocation*>& live_allocs);
    void Sweep();

    // template Find allocation
    template <bool IsFast>
    Allocation* FindAllocation(uintptr_t ptr) {
        if (ptr < allocated_memory_[0].ptr) {
            return nullptr;
        }
        Allocation fake{ptr, 0, nullptr, 0};

        std::vector<Allocation>::iterator begin_search = allocated_memory_.begin(),
                                          end_search = allocated_memory_.end();
        if constexpr (IsFast) {
            if (prev_find_ != allocated_memory_.end()) {
                if (prev_find_->ptr <= ptr) {
                    begin_search = prev_find_;
                } else {
                    end_search = prev_find_;
                }
            }
        }
        auto it = std::upper_bound(
            begin_search, end_search, fake,
            [](const Allocation& lhs, const Allocation& rhs) { return lhs.ptr < rhs.ptr; });
        if (it == allocated_memory_.begin()) {
            prev_find_ = allocated_memory_.end();
            return nullptr;
        }
        --it;
        prev_find_ = it;
        Allocation& alloc = *it;
        if (ptr < alloc.ptr + alloc.size) {
            return &alloc;
        }
        return nullptr;
    }

    std::vector<Allocation> allocated_memory_;
    std::vector<Allocation>::iterator prev_find_;  // for fast find alloc, like cached value
    size_t last_size_ = 0;
    size_t timer_;
    std::vector<GCRoot> roots_;
    GCScheduler scheduler_;
    bool enable_auto_ = true;

    std::atomic<bool> should_stop_ = false;
    std::atomic<size_t> stopped_ = 0;
    std::mutex lock_collect_, threads_registering_;
    std::condition_variable stopping_thread_;
    std::unordered_set<std::thread::id> threads_;
    std::atomic<size_t> threads_count_;
};
