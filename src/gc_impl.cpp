#include "gc_impl.h"
#include "gc.h"
#include "gc_fwd.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

uintptr_t Aligned(uintptr_t ptr) {
    return ptr + (kAlignment - ptr % kAlignment) % kAlignment;
}

static uintptr_t GetMemoryPtr(uintptr_t ptr) {
    void** mem_ptr = reinterpret_cast<void**>(ptr);
    return (mem_ptr == nullptr ? 0 : reinterpret_cast<uintptr_t>(*mem_ptr));
}

GCImpl::GCImpl() : timer_(0), scheduler_(this) {
}

void GCImpl::FreeAll() {
    std::lock_guard<std::mutex> lock(lock_collect_);
    for (const Allocation& allocation : allocated_memory_) {
        std::free(reinterpret_cast<void*>(allocation.ptr));
    }
    allocated_memory_.clear();
}

GCImpl::~GCImpl() {
    scheduler_.Shutdown();
    FreeAll();
}

void GCImpl::Init(const std::vector<GCRoot>& roots) {
    std::lock_guard<std::mutex> lock(lock_collect_);
    roots_ = roots;
}

void GCImpl::AddRoot(const GCRoot& root) {
    std::lock_guard<std::mutex> lock(lock_collect_);
    roots_.push_back(root);
}

bool operator==(const GCRoot& lhs, const GCRoot& rhs) {
    return lhs.addr == rhs.addr;
}

void GCImpl::DeleteRoot(const GCRoot& root) {
    std::lock_guard<std::mutex> lock(lock_collect_);
    std::erase(roots_, root);
}

void GCImpl::CreateAllocation(uintptr_t ptr, size_t size, FinalizerT finalizer) {
    Safepoint();
    std::unique_lock<std::mutex> lock(lock_collect_);
    allocated_memory_.push_back(Allocation{ptr, size, finalizer, timer_});
    if (enable_auto_) {
        scheduler_.UpdateAllocationStats(size);
    }
    ++timer_;
}

bool operator==(const Allocation& lhs, const Allocation& rhs) {
    return reinterpret_cast<uintptr_t>(lhs.ptr) == reinterpret_cast<uintptr_t>(rhs.ptr);
}

void GCImpl::DeleteAllocation(uintptr_t ptr) {
    std::lock_guard<std::mutex> lock(lock_collect_);
    Allocation fake_alloc{ptr, 0, nullptr, 0};
    std::erase(allocated_memory_, fake_alloc);
}

bool operator<(const Allocation& lhs, const Allocation& rhs) {
    return lhs.ptr < rhs.ptr;
}

void GCImpl::SortAllocations() {
    std::sort(allocated_memory_.begin() + last_size_, allocated_memory_.end());
    std::inplace_merge(allocated_memory_.begin(), allocated_memory_.begin() + last_size_,
                       allocated_memory_.end());
}

bool GCImpl::IsValidAllocation(const Allocation& alloc) {
    return alloc.last_valid_time >= timer_;
}

void* GCImpl::Malloc(size_t size, FinalizerT finalizer) {
    void* ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(reinterpret_cast<uintptr_t>(ptr), size, finalizer);
    return ptr;
}

void* GCImpl::Calloc(size_t nmemb, size_t size, FinalizerT finalizer) {
    void* ptr = std::calloc(nmemb, size);
    if (!ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(reinterpret_cast<uintptr_t>(ptr), nmemb * size, finalizer);
    return ptr;
}

void* GCImpl::Realloc(void* ptr, size_t size, FinalizerT finalizer) {
    DeleteAllocation(reinterpret_cast<uintptr_t>(ptr));
    void* new_ptr = std::realloc(ptr, size);
    if (!new_ptr) {
        throw std::bad_alloc{};
    }
    CreateAllocation(reinterpret_cast<uintptr_t>(new_ptr), size, finalizer);
    return new_ptr;
}

void GCImpl::Free(uintptr_t ptr) {
    Allocation* alloc = FindAllocation<false>(ptr);
    if (alloc == nullptr) {
        return;
    }
    alloc->finalizer(reinterpret_cast<void*>(alloc->ptr), alloc->size);
    std::free(reinterpret_cast<void*>(alloc->ptr));
    DeleteAllocation(alloc->ptr);
}

GCScheduler& GCImpl::GetScheduler() {
    return scheduler_;
}

const GCScheduler& GCImpl::GetScheduler() const {
    return scheduler_;
}

void GCImpl::DisableScheduler() {
    std::lock_guard<std::mutex> lock(threads_registering_);
    scheduler_.Stop();
    threads_.clear();
    threads_count_ = 0;
    enable_auto_ = false;
}

void GCImpl::EnableScheduler() {
    scheduler_.Start();
    enable_auto_ = true;
}

void GCImpl::Safepoint() {
    if (!should_stop_.load()) {
        return;
    }
    std::unique_lock<std::mutex> lock(lock_collect_);
    ++stopped_;
    stopping_thread_.wait(lock, [this]() { return !should_stop_.load(); });
    --stopped_;
}

void GCImpl::RegisterThread() {
    std::lock_guard<std::mutex> lock(threads_registering_);
    threads_.insert(std::this_thread::get_id());
    threads_count_ = threads_.size();
}

void GCImpl::DeregisterThread() {
    std::lock_guard<std::mutex> lock(threads_registering_);
    threads_.erase(std::this_thread::get_id());
    threads_count_ = threads_.size();
}

void GCImpl::StopWorld() {
    should_stop_ = true;
    while (stopped_ < threads_count_) {
        std::this_thread::yield();
    }
}

void GCImpl::ResumeWorld() {
    should_stop_ = false;
    stopping_thread_.notify_all();
}

void GCImpl::CollectPrepare() {
    ++timer_;
    SortAllocations();
    prev_find_ = allocated_memory_.end();
}

std::vector<Allocation*> GCImpl::MarkRoots() {
    std::vector<Allocation*> live;
    for (const auto& root : roots_) {
        uintptr_t start = reinterpret_cast<uintptr_t>(root.addr);
        uintptr_t end = start + root.size - kSize + 1;
        for (uintptr_t ptr = start; ptr < end; ptr += kSize) {
            Allocation* alloc = FindAllocation<true>(GetMemoryPtr(ptr));
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
            Allocation* heap_alloc = FindAllocation<true>(GetMemoryPtr(ptr));
            if (heap_alloc != nullptr) {
                heap_alloc->last_valid_time = timer_;
            }
        }
    }
}

void GCImpl::Sweep() {
    auto non_valid =
        std::stable_partition(allocated_memory_.begin(), allocated_memory_.end(),
                              [this](const Allocation& alloc) { return IsValidAllocation(alloc); });
    for (auto it = non_valid; it != allocated_memory_.end(); ++it) {
        Allocation& alloc = *it;
        alloc.finalizer(reinterpret_cast<void*>(alloc.ptr), alloc.size);
        std::free(reinterpret_cast<void*>(alloc.ptr));
    }
    allocated_memory_.erase(non_valid, allocated_memory_.end());
    last_size_ = allocated_memory_.size();
}

void GCImpl::Collect() {
    StopWorld();
    std::unique_lock<std::mutex> lock(lock_collect_);
    CollectPrepare();
    MarkHeapAllocs(MarkRoots());
    Sweep();
    lock.unlock();
    ResumeWorld();
}
