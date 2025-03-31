#include "gc.h"
#include "gc_impl.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

static std::unique_ptr<GCImpl> gc_instance = std::make_unique<GCImpl>();

Allocation ToAllocation(void* addr, size_t size) {
    return Allocation{reinterpret_cast<uintptr_t>(addr), size, BasicFinalizer, 0};   
}

#ifdef __cplusplus
extern "C" {
#endif

void BasicFinalizer(void*, size_t) {
}


void gc_init(GCRoot* roots, size_t num_roots) {
    std::vector<Allocation> root_vec;
    root_vec.reserve(num_roots);
    if (roots && num_roots > 0) {
        for (size_t i = 0; i < num_roots; ++i) {
            root_vec.emplace_back(ToAllocation(roots[i].addr, roots[i].size));
        }
    }
    gc_instance->Init(root_vec);
}

void* gc_malloc(size_t size, FinalizerT finalizer) {
    return gc_instance->Malloc(size, finalizer);
}

void* gc_malloc_default(size_t size) {
    return gc_malloc(size, BasicFinalizer);
}

void* gc_calloc(size_t nmemb, size_t size, FinalizerT finalizer) {
    return gc_instance->Calloc(nmemb, size, finalizer);
}

void* gc_calloc_default(size_t nmemb, size_t size) {
    return gc_calloc(nmemb, size, BasicFinalizer);
}

void* gc_realloc(void* ptr, size_t size, FinalizerT finalizer) {
    return gc_instance->Realloc(ptr, size, finalizer);
}

void* gc_realloc_default(void* ptr, size_t size) {
    return gc_realloc(ptr, size, BasicFinalizer);
}

void gc_free(void* ptr) {
    gc_instance->Free(reinterpret_cast<uintptr_t>(ptr));
}

void gc_free_all() {
    gc_instance->FreeAll();
}

void gc_collect() {
    gc_instance->GetScheduler().TriggerCollect();
}

void gc_wait_collect() {
    gc_instance->GetScheduler().WaitCollect();
}

void gc_collect_blocked() {
    gc_collect();
    gc_wait_collect();
}

void gc_add_root(GCRoot root) {
    gc_instance->AddRoot(ToAllocation(root.addr, root.size));
}

void gc_delete_root(GCRoot root) {
    gc_instance->DeleteRoot(ToAllocation(root.addr, root.size));
}

size_t gc_get_bytes_threshold() {
    return gc_instance->GetScheduler().GetThresholdBytes();
}

size_t gc_get_calls_threshold() {
    return gc_instance->GetScheduler().GetThresholdCalls();
}

size_t gc_get_collect_interval() {
    return gc_instance->GetScheduler().GetCollectionInterval().count();
}

void gc_set_bytes_threshold(size_t bytes) {
    gc_instance->GetScheduler().SetThresholdBytes(bytes);
}

void gc_set_calls_threshold(size_t calls) {
    gc_instance->GetScheduler().SetThresholdCalls(calls);
}

void gc_set_collect_interval(size_t milliseconds) {
    gc_instance->GetScheduler().SetCollectionInterval(std::chrono::milliseconds(milliseconds));
}

void gc_reset_info() {
    gc_instance->GetScheduler().ResetStats();
}

void gc_disable_auto() {
    gc_instance->DisableScheduler();
}
void gc_enable_auto() {
    gc_instance->EnableScheduler();
}

void gc_safepoint() {
    gc_instance->Safepoint();
}

void gc_register_thread() {
    gc_instance->RegisterThread();
}

void gc_deregister_thread() {
    gc_instance->DeregisterThread();
}

#ifdef __cplusplus
}
#endif
