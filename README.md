# GC-for-C

A modular, efficient, thread-safe garbage collector for C. Implements a Mark-Sweep algorithm with parallel marking, adaptive scheduling, and full runtime control.

## Overview

This project is a self-contained garbage collection system designed for integration into C/C++ programs without requiring compiler or runtime support. It provides a minimal API for memory allocation and garbage collection, enabling safer and more maintainable memory management in low-level systems.

Key features include:

- Manual and automatic garbage collection modes.
- Thread-safe stop-the-world collection with safepoints.
- Parallel marking with work-stealing.
- Allocation tracking with optimized lookup.
- Heuristic-based pacing and collection triggering.
- Full unit and performance test suite.

## Features

- **Mark-Sweep algorithm** — simple and effective, with no relocation.
- **Manual root management** — precise control over reachable objects.
- **Parallel GC marking** — utilizes all CPU cores for graph traversal.
- **Safepoints and synchronization** — robust stop-the-world coordination.
- **Optimized memory tracking** — fast binary search with caching and heuristics.
- **Adaptive GC scheduler** — based on allocation rate and thresholds.
- **Well-tested** — unit tests, stress tests, and performance benchmarks.

## Build Instructions

Requirements:

- CMake 3.15+
- C++20-compatible compiler

To build:

```bash
git clone https://github.com/pshpsh76/GC-for-C.git
cd GC-for-C
git submodule update --init --recursive
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Usage Example

```c
#include "gc.h"

int main() {
    gc_disable_auto();      // Manual collection mode
    gc_init(NULL, 0);       // No roots yet

    int* data = (int*) gc_malloc_default(100 * sizeof(int));

    // Work with GC-managed memory...

    gc_collect_blocked();   // Explicit GC
    gc_free_all();          // Cleanup
}
```

To register roots:

```c
GCRoot root = { &data, sizeof(data) };
gc_add_root(root);
```

## Tests and Benchmarks

To run unit tests:

```bash
ctest --output-on-failure
```

To run benchmarks(in build dir):
```bash
./tests/gc_benchmark
```

## Benchmark Highlights

| Scenario                                | Time per Iteration | Throughput           |
|----------------------------------------|--------------------|----------------------|
| `Realloc x100`                          | 43.4 µs            | 2.3M ops/sec         |
| `Drop 5% of 10k objects`                | 35.0 µs            | 349M objects/sec     |
| `Drop 100%`                             | 420 µs             | 23.5M objects/sec    |
| `Auto-GC: 1M allocations (32–64 bytes)` | 24.6 ms            | 29 iterations/sec    |

## Project Structure

```
include/        // Public GC API
src/            // Core GC implementation
tests/          // Unit and multithreaded tests & benchmarks
cmake/          // CMake module
external/       // External libs
```

## Design Highlights

- Efficient binary search over sorted allocations using `std::vector`.
- Caching of previous allocation lookups for temporal locality.
- Heap-range filtering to avoid unnecessary memory traversal.
- Adaptive scheduler with configurable thresholds and pacing.
- Parallel marking with independent work-stealing queues per thread.

## Use Cases

- Memory-safe subsystems in native applications.
- Custom interpreters or scripting languages.
- Embedded systems with constrained environments.
- Research and teaching in runtime systems or language design.

## Future Work

- Generational and compacting GC support.
- Weak references and finalizer mechanisms.
- Incremental or concurrent GC modes.
- Automatic root discovery via compiler metadata.
- Profiling and diagnostic tooling integration.

## License

This project is licensed under the [MIT License](LICENSE).

## Acknowledgements

Developed as a course project. Benchmark data and tests included to support reproducibility and further experimentation.
