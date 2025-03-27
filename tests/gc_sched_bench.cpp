#include <benchmark/benchmark.h>
#include <vector>
#include <random>

#include "gc.h"
#include "memory_actions.h"

static void BM_GcSimulateActionsAutomaticCollect(benchmark::State& state) {
    gc_disable_auto();
    PerformMemoryActions<false>(state, state.range(0), state.range(1), state.range(2));
}
BENCHMARK(BM_GcSimulateActionsAutomaticCollect)->Args({1000, 1, 10})->Args({10000, 64, 512})->Args({1000, 1024, 4096})->UseRealTime()->Unit(benchmark::kMicrosecond);
