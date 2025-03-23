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

    GCObject(bool is_root_value, char* memory_value);
    void AddEdge(const std::shared_ptr<GCObject>& obj);
};

class GenerationalGC {
public:
    void* Malloc(size_t size, bool is_root, void* parent);
    void Free(void* ptr);
    void MinorCollect();
    void MajorCollect();
    void Collect();
    GenerationalGC& GetInstance();
    ~GenerationalGC() = default;

private:
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_gen_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> old_gen_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> old_roots_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_roots_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_from_old_;
    int collections_count = 0;

    void IncCollectionsCount();
    void Mark(std::shared_ptr<GCObject> root);
    void Sweep(std::unordered_map<void*, std::shared_ptr<GCObject>>& generation);
    std::shared_ptr<GCObject> FindObject(void* ptr);
};
