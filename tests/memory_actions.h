#include <cstddef>
#include <vector>
#include <random>
#include <benchmark/benchmark.h>

#include "gc.h"

enum Action {
    Increment,
    Read,
    ChangeValue,
    Drop,
    Allocate,
};

template <bool CallCollect>
void PerformMemoryActions(benchmark::State& state, size_t num_objects, size_t min_size,
                          size_t max_size) {
    constexpr size_t kSeed = 204;
    constexpr size_t kActionsTypes = 5;
    constexpr size_t kActionNum = 200;
    std::mt19937 gen(kSeed);
    std::uniform_int_distribution<size_t> size_dist(min_size, max_size),
        actions_dist(0, kActionsTypes - 1), index_dist(0, num_objects - 1);

    std::vector<size_t> sizes(num_objects);

    char** root_array = new char*[num_objects];
    for (size_t i = 0; i < num_objects; ++i) {
        size_t alloc_size = size_dist(gen);
        char* ptr = static_cast<char*>(gc_malloc_default(alloc_size));
        sizes[i] = alloc_size;
        root_array[i] = ptr;
    }
    GCRoot root[] = {{static_cast<void*>(root_array), num_objects * sizeof(void*)}};
    gc_init(root, 1);
    if constexpr (!CallCollect) {
        gc_register_thread();
        gc_enable_auto();
    }
    for (auto _ : state) {
        if constexpr (CallCollect) {
            state.PauseTiming();
        }
        for (size_t i = 0; i < kActionNum; ++i) {
            if constexpr (!CallCollect) {
                gc_safepoint();
            }
            size_t ind;
            size_t try_cnt = 0;
            do {
                ++try_cnt;
                ind = index_dist(gen);
                if (try_cnt > 100) {
                    break;
                }
            } while (root_array[ind] == nullptr || sizes[ind] <= 1);
            if (try_cnt > 100) {
                break;
            }
            char* ptr = root_array[ind];
            switch (actions_dist(gen)) {
                case Action::Increment: {
                    ++ptr;
                    --sizes[ind];
                    root_array[ind] = ptr;
                    break;
                }
                case Action::Read: {
                    char res = *ptr;
                    (void)res;
                    break;
                }
                case Action::ChangeValue:
                    *ptr = 'a';
                    break;
                case Action::Drop:
                    root_array[ind] = nullptr;
                    sizes[ind] = 0;
                    break;
                case Action::Allocate: {
                    size_t future_sz = size_dist(gen);
                    root_array[ind] = static_cast<char*>(gc_malloc_default(future_sz));
                    sizes[ind] = future_sz;
                    break;
                }
                default:
                    break;
            }
        }
        if constexpr (CallCollect) {
            state.ResumeTiming();
            gc_collect_blocked();
        }
    }
    gc_disable_auto();
    for (size_t i = 0; i < num_objects; ++i) {
        root_array[i] = nullptr;
    }
    gc_collect_blocked();
    delete[] root_array;
}
