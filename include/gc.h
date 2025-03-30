#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FinalizerT)(void *, size_t);

void BasicFinalizer(void *ptr, size_t size);

typedef struct GCRoot {
    void *addr;
    size_t size;
} GCRoot;

void gc_init(GCRoot *roots, size_t num_roots);

// memory functions
void *gc_malloc(size_t size, FinalizerT finalizer);
void *gc_malloc_default(size_t size);
void *gc_calloc(size_t nmemb, size_t size, FinalizerT finalizer);
void *gc_calloc_default(size_t nmemb, size_t size);
void *gc_realloc(void *ptr, size_t size, FinalizerT finalizer);
void *gc_realloc_default(void *ptr, size_t size);
void gc_free(void *ptr);
void gc_free_all();

// trigger garbage collecting
void gc_collect();
void gc_wait_collect();
void gc_collect_blocked();

// root managing
void gc_add_root(GCRoot root);
void gc_delete_root(GCRoot root);

// managing for params of scheduler
size_t gc_get_bytes_threshold();
size_t gc_get_calls_threshold();
size_t gc_get_collect_interval();
void gc_set_bytes_threshold(size_t bytes);
void gc_set_calls_threshold(size_t calls);
void gc_set_collect_interval(size_t milliseconds);
void gc_reset_info();
void gc_disable_auto();
void gc_enable_auto();

// threads safety
void gc_safepoint();
void gc_register_thread();
void gc_deregister_thread();
#ifdef __cplusplus
}
#endif