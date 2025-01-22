#include <gtest/gtest.h>
#include "gc.h"

TEST(GarbageCollectorTest, SimpleAllocation) {
    void* ptr = gc_malloc(128);
    ASSERT_NE(ptr, nullptr);  // Ensure allocation succeeded
    gc_free(ptr);
}

TEST(GarbageCollectorTest, DoubleFree) {
    void* ptr = gc_malloc(64);
    gc_free(ptr);
    ASSERT_NO_THROW(gc_free(ptr));  // Ensure double-free is handled gracefully
}

TEST(GarbageCollectorTest, Collection) {
    gc_malloc(256);  // Allocate some memory
    ASSERT_NO_THROW(gc_collect());  // Ensure collection doesn't crash
}
