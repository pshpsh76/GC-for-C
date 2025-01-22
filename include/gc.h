#pragma once

#include <cstddef>

typedef void (*FinalizerT)(void *ptr, size_t size);

void BasicFinalizer(void* ptr, size_t size);

void* gc_malloc(size_t size, FinalizerT finalizer = BasicFinalizer);

void* gc_calloc(size_t nmemb, size_t size, FinalizerT finalizer = BasicFinalizer);

void* gc_realloc(void* ptr, size_t size, FinalizerT finalizer = BasicFinalizer);

void gc_free(void* ptr);

void gc_collect();