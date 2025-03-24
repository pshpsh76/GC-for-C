#include "gc_impl.h"
#include "gc.h"
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
    for (const auto& [ptr, allocation] : allocated_memory_) {
        std::free(ptr);
    }
    allocated_memory_.clear();
}

GCImpl::~GCImpl() {
    FreeAll();
}

void GCImpl::Init(const std::vector<GCRoot>& roots_param) {
    roots_ = roots_param;
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
    allocated_memory_[ptr] = {ptr, size, finalizer, timer_};
}

void GCImpl::DeleteAllocation(void* ptr) {
    allocated_memory_.erase(ptr);
}

Allocation* GCImpl::FindAllocation(void* ptr) {
    auto it = allocated_memory_.upper_bound(ptr);
    if (it == allocated_memory_.begin()) {
        return nullptr;
    }
    --it;
    Allocation& alloc = it->second;
    uintptr_t base = reinterpret_cast<uintptr_t>(alloc.ptr);
    uintptr_t target = reinterpret_cast<uintptr_t>(ptr);
    if (target >= base && target < base + alloc.size) {
        return &alloc;
    }
    return nullptr;
}

bool GCImpl::IsValidAllocation(Allocation* alloc) {
    return alloc->last_valid_time == timer_;
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
        uintptr_t end = start + root.size;
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
        uintptr_t heap_end = reinterpret_cast<uintptr_t>(alloc->ptr) + alloc->size;
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
        if (!IsValidAllocation(&it->second)) {
            Allocation alloc = it->second;
            it = allocated_memory_.erase(it);
            alloc.finalizer(alloc.ptr, alloc.size);
            std::free(alloc.ptr);
        } else {
            ++it;
        }
    }
}

void GCImpl::Collect() {
    ++timer_;
    MarkHeapAllocs(MarkRoots());
    Sweep();
}
