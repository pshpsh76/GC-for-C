#include "gc.h"
#include "gc_impl.h"
#include <cstddef>
#include <memory>
#include <vector>

static std::unique_ptr<GCImpl> gc_instance = std::make_unique<GCImpl>();

#ifdef __cplusplus
extern "C" {
#endif

void BasicFinalizer(void*, size_t) {
}

void gc_init(GCRoot* roots, size_t num_roots) {
    std::vector<GCRoot> root_vec;
    if (roots && num_roots > 0) {
        root_vec.assign(roots, roots + num_roots);
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
    gc_instance->Free(ptr);
}

void gc_collect() {
    gc_instance->Collect();
}

void gc_add_root(GCRoot root) {
    gc_instance->AddRoot(root);
}
void gc_delete_root(GCRoot root) {
    gc_instance->DeleteRoot(root);
}

#ifdef __cplusplus
}
#endif
