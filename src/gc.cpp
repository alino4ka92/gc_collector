#include "gc.h"
#include <gc_impl.h>

GenerationalGC& gc() {
    return GenerationalGC().GetInstance();
}

void* gc_malloc(size_t size, bool is_root, void* parent) {
    return gc().Malloc(size, is_root, parent);
}

void gc_free(void* ptr) {
    gc().Free(ptr);
}

void gc_collect() {
    gc().AutoCollect();
}
