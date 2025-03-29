#include <benchmark/benchmark.h>
#include <vector>

#include "gc.h"
#include "memory_actions.h"

static void BM_GcSimulateActionsAutomaticCollect(benchmark::State& state) {
    gc_disable_auto();
    PerformMemoryActions<false>(state, state.range(0), state.range(1), state.range(2));
}
BENCHMARK(BM_GcSimulateActionsAutomaticCollect)
    ->Args({1000000, 1, 10})
    ->Args({1000000, 64, 512})
    ->Args({50000, 1024, 4096})
    ->Args({100000, 1024, 8196})
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);
