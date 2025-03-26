#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include "gc.h"

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

    void Init(const std::vector<GCRoot>& roots);
    void AddRoot(const GCRoot& root);
    void DeleteRoot(const GCRoot& root);

    void* Malloc(size_t size, FinalizerT finalizer);
    void* Calloc(size_t nmemb, size_t size, FinalizerT finalizer);
    void* Realloc(void* ptr, size_t size, FinalizerT finalizer);
    void Free(uintptr_t ptr);

    void Collect();

private:
    void CreateAllocation(uintptr_t ptr, size_t size, FinalizerT finalizer);
    void DeleteAllocation(uintptr_t ptr);
    bool IsValidAllocation(const Allocation& alloc);
    void CollectPrepare();
    void SortAllocations();

    std::vector<Allocation*> MarkRoots();
    void MarkHeapAllocs(const std::vector<Allocation*>& live_allocs);
    void Sweep();
    void FreeAll();

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
    std::vector<Allocation>::iterator prev_find_;
    size_t timer_;
    std::vector<GCRoot> roots_;
};
