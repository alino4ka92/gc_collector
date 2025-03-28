#pragma once

#include <unordered_map>
#include <memory>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <condition_variable>

struct GCObject {
    std::atomic<bool> mark{false};
    bool is_root = false;
    std::mutex edges_mutex;
    std::unordered_set<std::shared_ptr<GCObject>> edges;
    std::shared_ptr<GCObject> parent = nullptr;
    std::unique_ptr<uint64_t[]> memory = nullptr;
    int size = 0;

    GCObject(bool is_root_value, size_t size);

    void AddEdge(const std::shared_ptr<GCObject> &obj);

    void RemEdge(const std::shared_ptr<GCObject> &obj);
};

class GenerationalGC {
public:
    GenerationalGC();
    GenerationalGC(const GenerationalGC&)=delete;
    GenerationalGC& operator=(const GenerationalGC&)=delete;
    void*Malloc(size_t size, bool is_root, void*parent);

    void ChangeParent(void* ptr, void* new_parent);

    void Free(void*ptr);

    void MinorCollect();

    void MajorCollect();

    static GenerationalGC &GetInstance();

    void ForceGarbageCollection(bool major);

    void ConfigureThresholds(size_t young_threshold, size_t old_threshold,
                             double young_ratio, double old_ratio);

    size_t GetCollectionsCount();

    size_t GetYoungGenSize();

    size_t GetOldGenSize();

    void StartGCThread();

    void StopGCThread();

    ~GenerationalGC();


private:
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_gen_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> old_gen_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> old_roots_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_roots_;
    std::unordered_map<void*, std::shared_ptr<GCObject>> young_from_old_;

    std::mutex young_gen_mutex_;
    std::mutex old_gen_mutex_;
    std::mutex gc_mutex_;

    std::atomic<size_t> young_gen_size_{0};
    std::atomic<size_t> old_gen_size_{0};
    std::atomic<size_t> total_allocated_bytes_{0};
    std::atomic<bool> gc_in_progress_{false};
    std::atomic<size_t> collections_count_{0};

    std::thread gc_thread_;
    std::atomic<bool> should_stop_{false};
    std::condition_variable gc_cv_;

    std::atomic<size_t> young_gen_threshold_ = 4 * 1024 * 1024;
    std::atomic<size_t> old_gen_threshold_ = 16 * 1024 * 1024;
    std::atomic<double> young_gen_ratio_ = 0.6;
    std::atomic<double> old_gen_ratio_ = 0.80;

    void GCThreadFunction();

    void IncCollectionsCount();

    void Mark(std::shared_ptr<GCObject> root);

    void Sweep(std::unordered_map<void*, std::shared_ptr<GCObject>> &generation);

    std::shared_ptr<GCObject> FindObject(void*ptr);

};
