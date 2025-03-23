#pragma once

#include <unordered_map>
#include <memory>
#include <cstddef>
#include <memory>
#include <unordered_set>

struct GCObject {
    bool mark = false;
    bool is_root = false;
    std::unordered_set<std::shared_ptr<GCObject>> edges;
    std::shared_ptr<char> memory = nullptr;
    int size=0;
    GCObject(bool is_root_value, char *memory_value);

    void AddEdge(const std::shared_ptr<GCObject> &obj);
};

class GenerationalGC {
public:
    void *Malloc(size_t size, bool is_root, void *parent);

    void Free(void *ptr);

    void MinorCollect();

    void MajorCollect();

    void Collect();

    GenerationalGC &GetInstance();

    void AutoCollect();

    void ConfigureThresholds(size_t young_threshold, size_t old_threshold,
                             double young_ratio, double old_ratio);

    ~GenerationalGC() = default;


private:
    std::unordered_map<void *, std::shared_ptr<GCObject>> young_gen_;
    std::unordered_map<void *, std::shared_ptr<GCObject>> old_gen_;
    std::unordered_map<void *, std::shared_ptr<GCObject>> old_roots_;
    std::unordered_map<void *, std::shared_ptr<GCObject>> young_roots_;
    std::unordered_map<void *, std::shared_ptr<GCObject>> young_from_old_;
    int collections_count = 0;

    void IncCollectionsCount();

    void Mark(std::shared_ptr<GCObject> root);

    void Sweep(std::unordered_map<void *, std::shared_ptr<GCObject>> &generation);

    std::shared_ptr<GCObject> FindObject(void *ptr);

    size_t total_allocated_bytes_ = 0;
    size_t young_gen_threshold_ = 4 * 1024 * 1024;
    size_t old_gen_threshold_ = 16 * 1024 * 1024;
    double young_gen_ratio_ = 0.6;
    double old_gen_ratio_ = 0.80;

    size_t young_gen_size_ = 0;
    size_t old_gen_size_ = 0;
};
