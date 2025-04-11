#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>

static std::atomic<int>& GetCounter() {
    static std::atomic<int> counter_finalizer = 0;
    return counter_finalizer;
}

static void wait_bit() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
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