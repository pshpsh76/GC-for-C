#include <benchmark/benchmark.h>
#include <vector>
#include <random>

#include "gc.h"
#include "memory_actions.h"

constexpr size_t kSeed = 204;

static void** SetupPartialWorkload(size_t num_objects, size_t fixed_size, double drop_probability) {
    std::mt19937 gen(kSeed);
    std::uniform_real_distribution<double> drop_dist(0.0, 1.0);

    void** root_array = new void*[num_objects];
    for (size_t i = 0; i < num_objects; ++i) {
        size_t alloc_size = fixed_size;
        void* ptr = gc_malloc_default(alloc_size);
        root_array[i] = ptr;
    }

    for (size_t i = 0; i < num_objects; ++i) {
        if (drop_dist(gen) < drop_probability) {
            root_array[i] = nullptr;
        }
    }
    return root_array;
}

static void BM_GcRealloc(benchmark::State& state) {
    gc_disable_auto();
    const size_t initial_size = 64;
    for (auto _ : state) {
        state.PauseTiming();
        void* ptr = gc_malloc_default(initial_size);
        state.ResumeTiming();
        for (int i = 0; i < 100; ++i) {
            size_t new_size = initial_size + (i % 10) * 10;
            ptr = gc_realloc_default(ptr, new_size);
        }
        gc_collect_blocked();
    }
    state.SetItemsProcessed(100 * state.iterations());
}
BENCHMARK(BM_GcRealloc)->UseRealTime()->MeasureProcessCPUTime()->Unit(benchmark::kMicrosecond);

static void BM_GcCollect_Drop5(benchmark::State& state) {
    gc_disable_auto();
    const size_t num_objects = 10000;
    const double drop_probability = 0.05;
    std::mt19937 gen(kSeed);
    std::uniform_real_distribution<double> drop_dist(0.0, 1.0);

    std::vector<void*> objects;
    objects.reserve(num_objects);
    for (size_t i = 0; i < num_objects; ++i) {
        objects.push_back(gc_malloc_default(64));
    }

    GCRoot root = {objects.data(), objects.size() * sizeof(void*)};
    gc_init(&root, 1);

    for (auto _ : state) {
        state.PauseTiming();
        for (size_t i = 0; i < objects.size(); ++i) {
            if (objects[i] != nullptr && drop_dist(gen) < drop_probability) {
                objects[i] = nullptr;
            }
        }

        state.ResumeTiming();

        gc_collect_blocked();
    }
    state.SetItemsProcessed(num_objects * state.iterations());
}
BENCHMARK(BM_GcCollect_Drop5)
    ->UseRealTime()
    ->MeasureProcessCPUTime()
    ->Unit(benchmark::kMicrosecond);

static void BM_GcCollect_DropProb(benchmark::State& state) {
    gc_disable_auto();
    const size_t num_objects = 10000;
    const size_t fixed_size = 64;
    double drop_probability = state.range(0) / 100.0;

    for (auto _ : state) {
        state.PauseTiming();
        void** root_array = SetupPartialWorkload(num_objects, fixed_size, drop_probability);
        GCRoot roots[] = {{reinterpret_cast<void*>(root_array), num_objects * sizeof(void*)}};
        gc_init(roots, 1);
        state.ResumeTiming();

        gc_collect_blocked();

        state.PauseTiming();
        delete[] root_array;
        state.ResumeTiming();
    }
    state.SetItemsProcessed(num_objects * state.iterations());
}
BENCHMARK(BM_GcCollect_DropProb)
    ->Arg(0)
    ->Arg(5)
    ->Arg(15)
    ->Arg(30)
    ->Arg(50)
    ->Arg(70)
    ->Arg(100)
    ->UseRealTime()
    ->MeasureProcessCPUTime()
    ->Unit(benchmark::kMicrosecond);

static void BM_GcSimulateActions(benchmark::State& state) {
    gc_disable_auto();
    PerformMemoryActions<true>(state, 10000, 64, 1024);
}
BENCHMARK(BM_GcSimulateActions)
    ->UseRealTime()
    ->MeasureProcessCPUTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
