#pragma once
#include <cstddef>
#include <cstdbool>

void* gc_malloc(size_t size, bool is_root, void* parent);

void gc_free(void* ptr);

void gc_collect(bool major);

void configure_thresholds(size_t young_threshold, size_t old_threshold,
                          double young_ratio, double old_ratio);

size_t get_collections_count();
size_t get_young_gen_size();
size_t get_old_gen_size();