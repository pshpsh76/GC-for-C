#include <chrono>
#include <cstddef>
#include <thread>
#include <gtest/gtest.h>
#include "gc.h"
#include "utils.h"

TEST(GСLibTest, SimpleAllocation) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    void* ptr = gc_malloc_default(128);
    ASSERT_NE(ptr, nullptr);

    gc_free(ptr);
}

TEST(GСLibTest, DoubleFree) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    void* ptr = gc_malloc_default(64);
    gc_free(ptr);
    ASSERT_NO_THROW(gc_free(ptr));
}

TEST(GСLibTest, Collection) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    gc_malloc_default(256);
    ASSERT_NO_THROW(gc_collect_blocked());
}

TEST(GСLibTest, LeakWithoutCollect) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    gc_malloc_default(128);
}

TEST(GСLibTest, RootNotCollected) {
    gc_disable_auto();
    int* arr = static_cast<int*>(gc_malloc_default(10 * sizeof(int)));
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }
    int* interior = arr + 3;
    GCRoot root = {reinterpret_cast<void*>(&interior), sizeof(void*)};
    gc_add_root(root);

    gc_collect_blocked();

    ASSERT_EQ(*interior, 3);
}

TEST(GСLibTest, LivingAllocationNotCollected) {
    gc_disable_auto();
    int* num;
    GCRoot roots[] = {{reinterpret_cast<void*>(&num), sizeof(num)}};
    gc_init(roots, 1);

    num = static_cast<int*>(gc_malloc_default(sizeof(*num)));
    *num = 12345;
    gc_collect_blocked();

    ASSERT_EQ(*num, 12345);
}

TEST(GСLibTest, Array) {
    gc_disable_auto();
    ResetCounter();
    int* array;
    GCRoot roots[] = {{reinterpret_cast<void*>(&array), sizeof(array)}};
    gc_init(roots, 1);

    constexpr size_t kArraySize = 500;
    array = static_cast<int*>(gc_calloc(kArraySize, sizeof(*array), CounterFinalizer));
    for (size_t i = 0; i < kArraySize; ++i) {
        array[i] = static_cast<int>(i);
    }

    constexpr size_t kOffset = 243;
    array += kOffset;
    ASSERT_EQ(GetCounter(), 0);
    gc_collect_blocked();
    ASSERT_EQ(GetCounter(), 0);
    for (size_t i = kOffset; i < kArraySize; ++i) {
        ASSERT_EQ(array[i - kOffset], static_cast<int>(i));
    }
    array = nullptr;
    gc_collect_blocked();
    ASSERT_EQ(GetCounter(), 1);
}

TEST(GСLibTest, ManyStackRoots) {
    gc_disable_auto();
    ResetCounter();
    const int num_objects = 1000;
    int* objects[num_objects];

    GCRoot roots[] = {{reinterpret_cast<void*>(objects), num_objects * sizeof(void*)}};
    gc_init(roots, 1);

    for (int i = 0; i < num_objects; ++i) {
        objects[i] = static_cast<int*>(gc_malloc(sizeof(int), CounterFinalizer));
        *objects[i] = i;
    }

    gc_collect_blocked();
    ASSERT_EQ(GetCounter(), 0);

    for (int i = 0; i < num_objects; ++i) {
        ASSERT_EQ(*objects[i], i);
        objects[i] = nullptr;
    }

    gc_collect_blocked();
    ASSERT_EQ(GetCounter(), num_objects);
}

TEST(GСLibTest, CyclicReferencesCollected) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    ResetCounter();

    Node* node1 = static_cast<Node*>(gc_malloc(sizeof(Node), CounterFinalizer));
    Node* node2 = static_cast<Node*>(gc_malloc(sizeof(Node), CounterFinalizer));
    node1->value = 1;
    node2->value = 2;

    node1->next = node2;
    node2->next = node1;

    node1 = nullptr;
    node2 = nullptr;

    gc_collect_blocked();

    ASSERT_EQ(GetCounter(), 2);
}

TEST(GСLibTest, ComplexDataStructureCollected) {
    gc_disable_auto();
    gc_init(nullptr, 0);

    ResetCounter();

    TreeNode* root = static_cast<TreeNode*>(gc_malloc(sizeof(TreeNode), CounterFinalizer));
    TreeNode* left = static_cast<TreeNode*>(gc_malloc(sizeof(TreeNode), CounterFinalizer));
    TreeNode* right = static_cast<TreeNode*>(gc_malloc(sizeof(TreeNode), CounterFinalizer));

    root->value = 10;
    left->value = 5;
    right->value = 15;

    root->left = left;
    root->right = right;

    left->right = root;

    gc_collect_blocked();

    ASSERT_EQ(GetCounter(), 3);
}

TEST(GСLibTest, Realloc) {
    gc_disable_auto();
    int* ptr;
    GCRoot roots[] = {{reinterpret_cast<void*>(&ptr), sizeof(void*)}};
    gc_init(roots, 1);

    ptr = static_cast<int*>(gc_malloc_default(sizeof(int) * 4));
    for (int i = 0; i < 4; i++) {
        ptr[i] = i;
    }

    ptr = static_cast<int*>(gc_realloc_default(ptr, sizeof(int) * 8));
    for (int i = 4; i < 8; i++) {
        ptr[i] = i;
    }

    gc_collect_blocked();

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(ptr[i], i);
    }
}