#include "gc_impl.h"
#include <stdexcept>

GCObject::GCObject(bool is_root_value, char *memory_value)
        : is_root(is_root_value), memory(memory_value) {
}

void GCObject::AddEdge(const std::shared_ptr<GCObject> &obj) {
    edges.insert(obj);
}


void* GenerationalGC::Malloc(size_t size, bool is_root, void* parent) {
    char* ptr = new char[size];
    auto obj = std::make_shared<GCObject>(is_root, ptr);
    obj->size = size;

    if (is_root) {
        young_roots_.insert({ptr, obj});
    }

    if (parent) {
        std::shared_ptr<GCObject> parent_obj = FindObject(parent);
        if (old_gen_.contains(parent_obj->memory.get())) {
            young_from_old_.insert({ptr, obj});
        }
        parent_obj->AddEdge(obj);
    }

    young_gen_.insert({ptr, obj});
    young_gen_size_ += size;
    total_allocated_bytes_ += size;
    AutoCollect();

    return ptr;
}
void GenerationalGC::Free(void *ptr) {
    std::shared_ptr<GCObject> obj = FindObject(ptr);
    obj->is_root = false;
    young_roots_.erase(ptr);
    old_roots_.erase(ptr);
}

void GenerationalGC::MinorCollect() {
    for (const auto &[ptr, obj]: young_roots_) {
        Mark(obj);
    }

    for (const auto &[ptr, obj]: young_from_old_) {
        Mark(obj);
    }

    Sweep(young_gen_);
    IncCollectionsCount();
}

void GenerationalGC::MajorCollect() {
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
    IncCollectionsCount();
}

void GenerationalGC::Collect() {
    if (collections_count % 3 == 0) {
        MajorCollect();
    } else {
        MinorCollect();
    }
}

void GenerationalGC::AutoCollect() {
    bool young_gen_full = young_gen_size_ >= young_gen_ratio_ * young_gen_threshold_ ;
    bool old_gen_full = old_gen_size_ >=  old_gen_ratio_ * old_gen_threshold_;

    if (old_gen_full) {
        MajorCollect();
        young_gen_size_ = 0;
        old_gen_size_ = 0;
        for (const auto& [ptr, obj] : old_gen_) {
            old_gen_size_ += obj->size;
        }
    }
    else if (young_gen_full) {
        MinorCollect();
        young_gen_size_ = 0;
        for (const auto& [ptr, obj] : young_gen_) {
            young_gen_size_ += obj->size;
        }
    }
}

void GenerationalGC::ConfigureThresholds(size_t young_threshold, size_t old_threshold,
                                         double young_ratio, double old_ratio) {
    young_gen_threshold_ = young_threshold;
    old_gen_threshold_ = old_threshold;
    young_gen_ratio_ = young_ratio;
    old_gen_ratio_ = old_ratio;
}

void GenerationalGC::IncCollectionsCount() {
    collections_count++;
    collections_count %= 3;
}

void GenerationalGC::Mark(std::shared_ptr<GCObject> root) {
    if (!root->mark) {
        root->mark = true;
        for (const auto &next_root: root->edges) {
            Mark(next_root);
        }
    }
}

void GenerationalGC::Sweep(std::unordered_map<void *, std::shared_ptr<GCObject>> &generation) {
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
        throw std::runtime_error("Object not found");
    }
}

GenerationalGC &GenerationalGC::GetInstance() {
    static GenerationalGC instance;
    return instance;
}
