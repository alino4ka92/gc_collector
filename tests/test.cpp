#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <algorithm>
#include "gc.h"

size_t YOUNG_THRESHOLD = 1024*1024; // 1024 KB
size_t OLD_THRESHOLD = 4*1024*1024; // 4096 KB
double YOUNG_RATIO = 0.6;
double OLD_RATIO = 0.8;

class GCBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        configure_thresholds(YOUNG_THRESHOLD, OLD_THRESHOLD, YOUNG_RATIO,  OLD_RATIO);
    }
};

TEST_F(GCBasicTest, BasicAllocationAndCollection) {
    void* ptr = gc_malloc(100, true, nullptr);
    ASSERT_NE(ptr, nullptr);

    gc_free(ptr);

    gc_collect(false);

    ASSERT_GE(get_collections_count(), 1);
}

TEST_F(GCBasicTest, MajorCollections) {
    void* root = gc_malloc(100, true, nullptr);
    gc_malloc(100, false, root);
    gc_malloc(100, false, root);

    gc_collect(true);

    gc_malloc(100, false, nullptr);

    gc_free(root);

    gc_collect(true);

    ASSERT_GE(get_collections_count(), 1);
}

TEST_F(GCBasicTest, YoungObjectsPromotion) {
    static const size_t large_object_size = 512 * 1024; // 512KB

    std::vector<void*> objects;
    for (int i = 0; i < 10; i++) {
        objects.push_back(gc_malloc(large_object_size, true, nullptr));
    }

    if (large_object_size * 10 > YOUNG_RATIO * YOUNG_THRESHOLD) {
        ASSERT_LT(get_young_gen_size(), large_object_size * objects.size());
        ASSERT_GT(get_old_gen_size(), 0);
    }

    for (size_t i = 0; i < objects.size() / 2; i++) {
        gc_free(objects[i]);
    }

    gc_collect(true);

    ASSERT_LT(get_old_gen_size(), large_object_size * objects.size());
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
