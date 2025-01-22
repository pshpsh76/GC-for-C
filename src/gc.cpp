#include "gc.h"
#include <stdexcept>

void BasicFinalizer(void* ptr, size_t size) {
    throw std::runtime_error{"Not implemented"};
}

void* gc_malloc(size_t size, FinalizerT finalizer) {
    throw std::runtime_error{"Not implemented"};
}

void* gc_calloc(size_t nmemb, size_t size, FinalizerT finalizer) {
    throw std::runtime_error{"Not implemented"};
}

void* gc_realloc(void* ptr, size_t size, FinalizerT finalizer) {
    throw std::runtime_error{"Not implemented"};
}

void gc_free(void* ptr) {
    throw std::runtime_error{"Not implemented"};
}

void gc_collect() {
    throw std::runtime_error{"Not implemented"};
}