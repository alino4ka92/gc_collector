#pragma once
#include <cstddef>
#include <cstdbool>

void* gc_malloc(size_t size, bool is_root, void* parent);

void gc_free(void* ptr);

void gc_collect();



