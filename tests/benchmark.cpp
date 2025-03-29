#include "gc.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include <algorithm>
#include <chrono>

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
const size_t PERSISTENT_OBJECT_SIZE = 1024;

static void CycleAllocations(benchmark::State &state) {
    const int iterations = state.range(0);
    const int persistent_objects = state.range(1);
    const int temp_objects_per_iteration = state.range(2);
    for (auto _: state) {
        state.PauseTiming();
        gc_collect(true); // ждем очистку мусора с предыдущей стадии
        state.ResumeTiming();
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
            temp_objects.reserve(temp_objects_per_iteration);
            for (int j = 0; j < temp_objects_per_iteration; ++j) {
                void *parent = nullptr;
                if (!persistent.empty()) {
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
        ->Args({10000, 128}) // 10000 objects 128 B each
        ->Args({10000, 1024}) // 10000 objects 1 KB each
        ->Args({100000, 128}) // 100000 objects 128 B each
        ->Args({100000, 1024}) // 100000 objects 1 KB each
        ->Unit(benchmark::kMillisecond)
        ->Name("LargeAllocations")->MeasureProcessCPUTime();

BENCHMARK(CycleAllocations)
        ->Args({1000, 10, 10}) // 1000 iterations, 10 persistent objects, 10 temporary objects
        ->Args({1000, 10, 100}) // 1000 iterations, 10 persisent objects, 100 temporary objects
        ->Args({10000, 10000, 50}) // 10000 iterations, 10000 persistent objects, 50 temporary objects
        ->Args({10000, 10000, 100}) // 10000 iterations, 10000 persisent objects, 100 temporary objects
        ->Args({10000, 100000, 50}) // 10000 iterations, 100000 persisent objects, 50 temporary objects
        ->Unit(benchmark::kMillisecond)
        ->Name("CycleAllocations")->MeasureProcessCPUTime()->Iterations(10);

BENCHMARK_MAIN();
