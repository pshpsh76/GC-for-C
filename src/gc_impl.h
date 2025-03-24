#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include "gc.h"

struct Allocation {
    void* ptr;
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
    void Free(void* ptr);

    void Collect();

private:
    void CreateAllocation(void* ptr, size_t size, FinalizerT finalizer);
    void DeleteAllocation(void* ptr);
    bool IsValidAllocation(Allocation* alloc);

    std::vector<Allocation*> MarkRoots();
    void MarkHeapAllocs(const std::vector<Allocation*>& live_allocs);
    void Sweep();
    void FreeAll();

    Allocation* FindAllocation(void* ptr);

    std::map<void*, Allocation>
        allocated_memory_;  // Try to change it to vector to better cache locality
    size_t timer_;
    std::vector<GCRoot> roots_;
};
