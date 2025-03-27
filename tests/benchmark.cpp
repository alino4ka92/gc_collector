#include "gc.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
int cnt=0;
static void LargeAllocations(benchmark::State &state) {
    const size_t block_size = state.range(0);
    const size_t object_size = state.range(1);

    for (auto _: state) {
        state.PauseTiming();
        std::vector<void *> objects;
        objects.reserve(block_size);
        std::vector<size_t> indices(block_size);
        for (size_t i = 0; i < block_size; ++i) {
            indices[i] = i;
        }
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(indices.begin(), indices.end(), g);

        state.ResumeTiming();

        for (size_t i = 0; i < block_size; ++i) {
            bool is_root = (i % 5 == 0);
            void *parent = nullptr;
            if (i % 3 == 0 && !objects.empty()) {
                parent = objects[i - 1];
            }
            void *ptr = gc_malloc(object_size, is_root, parent);
            objects.push_back(ptr);
        }
        for (size_t i = 0; i < block_size / 2; ++i) {
            gc_free(objects[indices[i]]);
            objects[indices[i]] = nullptr;
        }
        benchmark::ClobberMemory();
    }
}

const size_t TEMP_OBJECT_SIZE = 10;
const size_t PERSISTENT_OBJECT_SIZE = 512;
const size_t TEMP_OBJECTS_PER_ITERATION = 10;

static void CycleAllocations(benchmark::State &state) {
    const int iterations = state.range(0);
    const int persistent_objects = state.range(1);

    for (auto _: state) {
        std::vector<void *> persistent;
        persistent.reserve(persistent_objects);

        for (int i = 0; i < persistent_objects; ++i) {
            void *ptr = gc_malloc(PERSISTENT_OBJECT_SIZE, true, nullptr);
            persistent.push_back(ptr);
            if (i % 3 == 0) {
                void *child = gc_malloc(PERSISTENT_OBJECT_SIZE, false, ptr);
                persistent.push_back(child);
            }
        }


        for (int iter = 0; iter < iterations; ++iter) {
            std::vector<void *> temp_objects;
            temp_objects.reserve(TEMP_OBJECTS_PER_ITERATION);

            for (int j = 0; j < TEMP_OBJECTS_PER_ITERATION; ++j) {
                void *parent = nullptr;
                if (!persistent.empty() && j % 2 == 0) {
                    parent = persistent[j % persistent.size()];
                }

                void *temp = gc_malloc(TEMP_OBJECT_SIZE, false, parent);
                temp_objects.push_back(temp);

                if (j > 0 && j % 3 == 0) {
                    void *child = gc_malloc(TEMP_OBJECT_SIZE / 2, false, temp);
                    temp_objects.push_back(child);
                }
            }

            benchmark::DoNotOptimize(temp_objects.data());

            for (auto *obj: temp_objects) {
                gc_free(obj);
            }
        }

        benchmark::ClobberMemory();
        for (auto *obj: persistent) {
            gc_free(obj);
        }

    }
}

BENCHMARK(LargeAllocations)
  //      ->Args({1000, 128})    // 1000 objects 128 B each
  //      ->Args({1000, 512})   // 1000 objects 1 KB each
        ->Args({10000, 128})   // 10000 objects 128 B each
        ->Args({10000, 128})  // 10000 objects 1 KB each
        ->Unit(benchmark::kMillisecond)
        ->Name("LargeAllocations");

BENCHMARK(CycleAllocations)
        ->Args({100, 10})     // 100 iterations, 10 persistent objects
        ->Args({100, 100})    // 100 iterations, 100 persistent objects
        ->Args({1000, 10})    // 1000 iterations, 10 persisent objects
        ->Args({1000, 100})   // 1000 iterations, 100 persistent objects
        ->Unit(benchmark::kMillisecond)
        ->Name("CycleAllocations");


BENCHMARK_MAIN();
