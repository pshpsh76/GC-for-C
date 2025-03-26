#include "gc_impl.h"
#include "gc.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

uintptr_t Aligned(uintptr_t ptr) {
    return ptr + (kAlignment - ptr % kAlignment) % kAlignment;
}

static void* GetMemoryPtr(uintptr_t ptr) {
    void** mem_ptr = reinterpret_cast<void**>(ptr);
    return (mem_ptr == nullptr ? mem_ptr : *mem_ptr);
}

GCImpl::GCImpl() : timer_(0) {
}

void GCImpl::FreeAll() {
    for (const Allocation& allocation : allocated_memory_) {
        std::free(allocation.ptr);
    }
    allocated_memory_.clear();
}

GCImpl::~GCImpl() {
    FreeAll();
}

void GCImpl::Init(const std::vector<GCRoot>& roots) {
    roots_ = roots;
}

void GCImpl::AddRoot(const GCRoot& root) {
    roots_.push_back(root);
}

bool operator==(const GCRoot& lhs, const GCRoot& rhs) {
    return lhs.addr == rhs.addr;
}

void GCImpl::DeleteRoot(const GCRoot& root) {
    std::erase(roots_, root);
}

void GCImpl::CreateAllocation(void* ptr, size_t size, FinalizerT finalizer) {
    allocated_memory_.push_back(Allocation{ptr, size, finalizer, timer_});
}

bool operator==(const Allocation& lhs, const Allocation& rhs) {
    return lhs.ptr == rhs.ptr;
}

void GCImpl::DeleteAllocation(void* ptr) {
    Allocation fake_alloc{ptr, 0, nullptr, 0};
    std::erase(allocated_memory_, fake_alloc);
}

void GCImpl::SortAllocations() {
    std::sort(allocated_memory_.begin(), allocated_memory_.end(),
              [](const Allocation& lhs, const Allocation& rhs) { return lhs.ptr < rhs.ptr; });
}

Allocation* GCImpl::FindAllocation(void* ptr) {
    Allocation fake{ptr, 0, nullptr, 0};
    auto it = std::upper_bound(
        allocated_memory_.begin(), allocated_memory_.end(), fake,
        [](const Allocation& lhs, const Allocation& rhs) { return lhs.ptr < rhs.ptr; });
    if (it == allocated_memory_.begin()) {
        return nullptr;
    }
    --it;
    Allocation& alloc = *it;
    uintptr_t base = reinterpret_cast<uintptr_t>(alloc.ptr);
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    if (target >= base && target < base + alloc.size) {
        return &alloc;
    }
    return nullptr;
}

bool GCImpl::IsValidAllocation(const Allocation& alloc) {
    return alloc.last_valid_time == timer_;
}

void* GCImpl::Malloc(size_t size, FinalizerT finalizer) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(ptr, size, finalizer);
    return ptr;
}

void* GCImpl::Calloc(size_t nmemb, size_t size, FinalizerT finalizer) {
    void* ptr = std::calloc(nmemb, size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(ptr, nmemb * size, finalizer);
    return ptr;
}

void* GCImpl::Realloc(void* ptr, size_t size, FinalizerT finalizer) {
    DeleteAllocation(ptr);
    void* new_ptr = std::realloc(ptr, size);
    if (!new_ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(new_ptr, size, finalizer);
    return new_ptr;
}

void GCImpl::Free(void* ptr) {
    Allocation* alloc = FindAllocation(ptr);
    if (alloc == nullptr) {
        return;
    }
    alloc->finalizer(alloc->ptr, alloc->size);
    std::free(alloc->ptr);
    DeleteAllocation(alloc->ptr);
}

std::vector<Allocation*> GCImpl::MarkRoots() {
    std::vector<Allocation*> live;
    for (const auto& root : roots_) {
        uintptr_t start = reinterpret_cast<uintptr_t>(root.addr);
        uintptr_t end = start + root.size - kSize + 1;
        for (uintptr_t ptr = start; ptr < end; ptr += kSize) {
            Allocation* alloc = FindAllocation(GetMemoryPtr(ptr));
            if (alloc != nullptr) {
                alloc->last_valid_time = timer_;
                if (alloc->size >= kSize) {
                    live.push_back(alloc);
                }
            }
        }
    }
    return live;
}

void GCImpl::MarkHeapAllocs(const std::vector<Allocation*>& live_allocs) {
    for (Allocation* alloc : live_allocs) {
        uintptr_t heap_start = Aligned(reinterpret_cast<uintptr_t>(alloc->ptr));
        uintptr_t heap_end = reinterpret_cast<uintptr_t>(alloc->ptr) + alloc->size - kSize + 1;
        for (uintptr_t ptr = heap_start; ptr < heap_end; ptr += kSize) {
            Allocation* heap_alloc = FindAllocation(GetMemoryPtr(ptr));
            if (heap_alloc != nullptr) {
                heap_alloc->last_valid_time = timer_;
            }
        }
    }
}

void GCImpl::Sweep() {
    for (auto it = allocated_memory_.begin(); it != allocated_memory_.end();) {
        if (!IsValidAllocation(*it)) {
            std::swap(*it, allocated_memory_.back());
            Allocation alloc = allocated_memory_.back();
            alloc.finalizer(alloc.ptr, alloc.size);
            std::free(alloc.ptr);
            allocated_memory_.pop_back();
        } else {
            ++it;
        }
    }
}

void GCImpl::Collect() {
    ++timer_;
    SortAllocations();
    MarkHeapAllocs(MarkRoots());
    Sweep();
}
