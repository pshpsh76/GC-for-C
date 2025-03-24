#include <benchmark/benchmark.h>
#include <vector>
#include <random>

#include "gc.h"

static void** SetupPartialWorkload(size_t num_objects, size_t fixed_size, bool use_fixed_size,
                                   size_t min_size, size_t max_size, double drop_probability) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> size_dist(min_size, max_size);
    std::uniform_real_distribution<double> drop_dist(0.0, 1.0);

    std::vector<void*> objects;
    objects.reserve(num_objects);
    for (size_t i = 0; i < num_objects; ++i) {
        size_t alloc_size = use_fixed_size ? fixed_size : size_dist(gen);
        void* ptr = gc_malloc_default(alloc_size);
        objects.push_back(ptr);
    }

    void** root_array = new void*[num_objects];
    for (size_t i = 0; i < num_objects; ++i) {
        root_array[i] = objects[i];
    }

    for (size_t i = 0; i < num_objects; ++i) {
        if (drop_dist(gen) < drop_probability) {
            root_array[i] = nullptr;
        }
    }
    return root_array;
}

static void BM_GcCollect_Unreachable(benchmark::State& state) {
    const size_t num_objects = 10000;
    for (auto _ : state) {
        state.PauseTiming();
        std::vector<void*> objects;
        objects.reserve(num_objects);
        for (size_t i = 0; i < num_objects; ++i) {
            objects.push_back(gc_malloc_default(64));
        }

        for (size_t i = 0; i < num_objects; ++i) {
            objects[i] = nullptr;
        }

        gc_init(nullptr, 0);
        state.ResumeTiming();

        gc_collect();

        state.PauseTiming();
        state.ResumeTiming();
    }
    state.SetItemsProcessed(num_objects * state.iterations());
}
BENCHMARK(BM_GcCollect_Unreachable)->Unit(benchmark::kMicrosecond);

static void BM_GcCollect_Partial(benchmark::State& state) {
    const size_t num_objects = 10000;
    const double drop_probability = 0.3;
    const size_t fixed_size = 64;

    for (auto _ : state) {
        state.PauseTiming();

        void** root_array =
            SetupPartialWorkload(num_objects, fixed_size, true, 0, 0, drop_probability);
        GCRoot roots[] = {{reinterpret_cast<void*>(root_array), num_objects * sizeof(void*)}};
        gc_init(roots, 1);
        state.ResumeTiming();

        gc_collect();

        state.PauseTiming();
        delete[] root_array;
        state.ResumeTiming();
    }
    state.SetItemsProcessed(num_objects * state.iterations());
}
BENCHMARK(BM_GcCollect_Partial)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
