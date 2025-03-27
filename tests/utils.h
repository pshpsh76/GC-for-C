#pragma once

#include <atomic>
#include <cstddef>

static std::atomic<int>& GetCounter() {
    static std::atomic<int> counter_finalizer = 0;
    return counter_finalizer;
}

static void ResetCounter() {
    GetCounter() = 0;
}

extern "C" {
static void CounterFinalizer(void*, size_t) {
    ++GetCounter();
}
}

struct Node {
    Node* next;
    int value;
};

struct TreeNode {
    TreeNode* left;
    TreeNode* right;
    int value;
};