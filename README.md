# C++ Garbage Collector

This repository contains a simple C++ garbage collector implementation that automatically manages memory by freeing
unreferenced objects. It features a two-generational collection system (young and old), configurable thresholds, object
relationship tracking, root object management, and supports both major and minor collection cycles.

## API Reference

```cpp
// Allocate memory of specified size
// is_root: whether this object should be treated as a root object
// parent: pointer to parent object (NULL if none)
void* gc_malloc(size_t size, bool is_root, void* parent);

// Free an object
void gc_free(void* ptr);

// Run garbage collection
// major: if true, collect both generations; if false, collect only young generation
void gc_collect(bool major);

// Configure garbage collection parameters
// young_threshold: maximum size for young generation before collection
// old_threshold: maximum size for old generation before collection
// young_ratio: percentage of young generation occupancy that triggers collection
// old_ratio: percentage of old generation occupancy that triggers collection
void configure_thresholds(size_t young_threshold, size_t old_threshold,
double young_ratio, double old_ratio);

// Change the parent of an object
void change_parent(void* ptr, void* new_parent_ptr);

// Get statistics
size_t get_collections_count();
size_t get_young_gen_size();
size_t get_old_gen_size();
```

## Building the Project

### Prerequisites

- Linux/MacOS
- GCC or Clang compiler
- CMake
- Google Test
- Google Benchmark

### Build Instructions

Clone the repository and build using cmake:

```bash
git clone https://github.com/alino4ka92/gc_collector
cd gc_collector
mkdir build
cd build
cmake ..
```

### Run tests

```bash
# Run this from build directory
make GcTests
tests/GcTests
```

### Run benchmark

```bash
# Run this from build directory
make Benchmark
tests/Benchmark
```
