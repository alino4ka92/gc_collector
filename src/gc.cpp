#include "gc.h"
#include <gc_impl.h>

GenerationalGC& gc() {
    return GenerationalGC::GetInstance();
}

void* gc_malloc(size_t size, bool is_root, void* parent) {
    return gc().Malloc(size, is_root, parent);
}

void gc_free(void* ptr) {
    gc().Free(ptr);
}

void gc_collect(bool major) {
    gc().ForceGarbageCollection(major);
}

void change_parent(void* ptr, void* new_parent_ptr) {
    gc().ChangeParent(ptr, new_parent_ptr);
}

void configure_thresholds(size_t young_threshold, size_t old_threshold,
                          double young_ratio, double old_ratio) {
    gc().ConfigureThresholds(young_threshold, old_threshold, young_ratio, old_ratio);
}


size_t get_collections_count() {
    return gc().GetCollectionsCount();
}

size_t get_old_gen_size() {
    return gc().GetOldGenSize();
}
size_t get_young_gen_size() {
    return gc().GetYoungGenSize();
}