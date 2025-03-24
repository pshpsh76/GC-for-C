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
void *gc_malloc(size_t size, FinalizerT finalizer);
void *gc_malloc_default(size_t size);
void *gc_calloc(size_t nmemb, size_t size, FinalizerT finalizer);
void *gc_calloc_default(size_t nmemb, size_t size);
void *gc_realloc(void *ptr, size_t size, FinalizerT finalizer);
void *gc_realloc_default(void *ptr, size_t size);
void gc_free(void *ptr);
void gc_collect();
void gc_add_root(GCRoot root);
void gc_delete_root(GCRoot root);

#ifdef __cplusplus
}
#endif