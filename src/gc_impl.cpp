#include "gc_impl.h"
#include <stdexcept>
#include <iostream>
#include <mutex>
#include <vector>

constexpr int TIME_TO_CHECK = 1000;

GCObject::GCObject(bool is_root_value, size_t size)
        : is_root(is_root_value), memory(std::make_unique<uint64_t[]>(size / 8 + 1)) {
}

void GCObject::AddEdge(const std::shared_ptr<GCObject> &obj) {
    std::unique_lock<std::mutex> lock(edges_mutex);
    edges.insert(obj->memory.get());
}

void GCObject::RemEdge(const std::shared_ptr<GCObject> &obj) {
    std::unique_lock lock(edges_mutex);
    edges.erase(obj->memory.get());
}

GenerationalGC::GenerationalGC() {
    StartGCThread();
}

GenerationalGC::~GenerationalGC() {
    StopGCThread();
}

void GenerationalGC::StartGCThread() {
    should_stop_.store(false);
    gc_thread_ = std::thread(&GenerationalGC::GCThreadFunction, this);
}

void GenerationalGC::StopGCThread() {
    should_stop_.store(true);
    gc_cv_.notify_one();
    if (gc_thread_.joinable()) {
        gc_thread_.join();
    }
}

void GenerationalGC::ForceGarbageCollection(bool major) {
    if (major) {
        MajorCollect();
    } else {
        MinorCollect();
    }
}


void GenerationalGC::GCThreadFunction() {
    while (!should_stop_.load()) {
        {
            std::unique_lock<std::mutex> lock(background_mutex_);
            gc_cv_.wait_for(lock, std::chrono::milliseconds(TIME_TO_CHECK), [this] {
                return should_stop_.load();
            });
        }
        if (should_stop_.load()) {
            break;
        }

        bool young_gen_full = static_cast<double>(young_gen_size_.load()) >=
                              young_gen_ratio_ * static_cast<double>(young_gen_threshold_);
        bool old_gen_full = collections_count_.load() % 5 == 0 ||
                            static_cast<double>(old_gen_size_.load()) >=
                            old_gen_ratio_ * static_cast<double>(old_gen_threshold_);
        if (old_gen_full) {
            MajorCollect();
        } else if (young_gen_full) {
            MinorCollect();
        }
    }
}

void *GenerationalGC::Malloc(size_t size, bool is_root, void *parent) {
    auto obj = std::make_shared<GCObject>(is_root, size);
    void *ptr = obj->memory.get();
    obj->size = size;
    {
        std::lock_guard<std::mutex> lock(gc_mutex_);
        if (is_root) {
            young_roots_.insert({ptr, obj});
        }
        if (parent) {
            std::shared_ptr<GCObject> parent_obj = nullptr;
            if (young_gen_.contains(parent)) {
                parent_obj = young_gen_[parent];
            } else {
                if (old_gen_.contains(parent)) {
                    parent_obj = old_gen_[parent];
                    young_from_old_.insert({ptr, obj});
                }
            }
            if (parent_obj) {
                parent_obj->AddEdge(obj);
                obj->parent = parent;
            }
        }
        young_gen_.insert({ptr, obj});
    }

    young_gen_size_ += size;
    return ptr;
}

void GenerationalGC::ChangeParent(void *ptr, void *new_parent) {
    {
        std::lock_guard<std::mutex> lock(gc_mutex_);

        std::shared_ptr<GCObject> obj = FindObject(ptr);

        std::shared_ptr<GCObject> old_parent_obj = FindObject(obj->parent);
        if (old_parent_obj) {
            old_parent_obj->RemEdge(obj);
        }

        std::shared_ptr<GCObject> new_parent_obj = FindObject(new_parent);
        new_parent_obj->AddEdge(obj);

        obj->parent = new_parent;
    }
}

void GenerationalGC::Free(void *ptr) {
    try {
        std::lock_guard<std::mutex> lock(gc_mutex_);

        std::shared_ptr<GCObject> obj = FindObject(ptr);
        if (obj) {
            obj->is_root = false;
            young_roots_.erase(ptr);
            old_roots_.erase(ptr);
        }
    } catch (const std::runtime_error &) {
    }
}

size_t GenerationalGC::GetCollectionsCount() {
    return collections_count_.load();
}

size_t GenerationalGC::GetYoungGenSize() {
    return young_gen_size_.load();
}

size_t GenerationalGC::GetOldGenSize() {
    return old_gen_size_.load();
}

void GenerationalGC::MinorCollect() {
    bool expected = false;
    if (!gc_in_progress_.compare_exchange_strong(expected, true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(gc_mutex_);
        for (const auto &[ptr, obj]: young_roots_) {
            Mark(obj);
        }
        for (const auto &[ptr, obj]: young_from_old_) {
            Mark(obj);
        }

        Sweep(young_gen_);
        young_gen_size_ = 0;
        for (const auto &[ptr, obj]: young_gen_) {
            young_gen_size_ += obj->size;
        }
    }

    IncCollectionsCount();
    gc_in_progress_.store(false);
}

void GenerationalGC::MajorCollect() {
    bool expected = false;
    if (!gc_in_progress_.compare_exchange_strong(expected, true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(gc_mutex_);
        for (const auto &[ptr, obj]: old_roots_) {
            Mark(obj);
        }
        for (const auto &[ptr, obj]: young_roots_) {
            Mark(obj);
        }

        Sweep(old_gen_);
        Sweep(young_gen_);


        for (const auto &[ptr, obj]: young_gen_) {
            old_gen_.insert({ptr, obj});
            if (obj->is_root) {
                old_roots_.insert({ptr, obj});
            }
        }

        young_gen_.clear();
        young_from_old_.clear();

        young_gen_size_ = 0;
        old_gen_size_ = 0;

        for (const auto &[ptr, obj]: old_gen_) {
            old_gen_size_ += obj->size;
        }
    }

    IncCollectionsCount();
    gc_in_progress_.store(false);
}


void GenerationalGC::ConfigureThresholds(size_t young_threshold, size_t old_threshold,
                                         double young_ratio, double old_ratio) {
    young_gen_threshold_ = young_threshold;
    old_gen_threshold_ = old_threshold;
    young_gen_ratio_ = young_ratio;
    old_gen_ratio_ = old_ratio;
}

void GenerationalGC::IncCollectionsCount() {
    collections_count_.fetch_add(1);
}

void GenerationalGC::Mark(std::shared_ptr<GCObject> root) {
    std::vector<std::shared_ptr<GCObject> > stack;
    stack.push_back(root);

    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();
        if (!current->mark) {
            current->mark = true;
            std::lock_guard<std::mutex> edge_lock(current->edges_mutex);
            for (const auto &next: current->edges) {
                auto obj = FindObject(next);
                if (!obj->mark) {
                    stack.push_back(obj);
                }
            }
        }
    }
}

void GenerationalGC::Sweep(std::unordered_map<void *, std::shared_ptr<GCObject> > &generation) {
    auto it = generation.begin();
    while (it != generation.end()) {
        if (!(it->second)->mark) {
            it = generation.erase(it);
        } else {
            (it->second)->mark = false;
            ++it;
        }
    }
}

std::shared_ptr<GCObject> GenerationalGC::FindObject(void *ptr) {
    if (young_gen_.contains(ptr)) {
        return young_gen_[ptr];
    } else if (old_gen_.contains(ptr)) {
        return old_gen_[ptr];
    } else {
        return nullptr;
    }
}

GenerationalGC &GenerationalGC::GetInstance() {
    static GenerationalGC instance;
    return instance;
}
