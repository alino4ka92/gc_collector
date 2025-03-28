#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
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

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_GE(get_collections_count(), 1);
}

TEST_F(GCBasicTest, YoungObjectsPromotion) {
    static const size_t large_object_size = 512 * 1024; // 512KB

    std::vector<void*> objects;
    for (int i = 0; i < 10; i++) {
        objects.push_back(gc_malloc(large_object_size, true, nullptr));
    }

    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (large_object_size * 10 > YOUNG_RATIO * YOUNG_THRESHOLD) {
        ASSERT_LT(get_young_gen_size(), large_object_size * objects.size());
        ASSERT_GT(get_old_gen_size(), 0);
    }

    for (size_t i = 0; i < objects.size() / 2; i++) {
        gc_free(objects[i]);
    }

    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_LT(get_old_gen_size(), large_object_size * objects.size());

    for (size_t i = objects.size()/2; i < objects.size(); i++) {
        gc_free(objects[i]);
    }
}

TEST_F(GCBasicTest, CollectionCycle) {
    gc_collect(true);

    size_t initial_count = get_collections_count();
    size_t initial_old_gen = get_old_gen_size();
    for (int cycle = 0; cycle < 4; cycle++) {

        std::vector<void*> objects;
        for (int i = 0; i < 5; i++) {
            objects.push_back(gc_malloc(1024*500, true, nullptr));
        }

        for (size_t i = 0; i < objects.size() / 2; i++) {
            gc_free(objects[i]);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_GT(get_collections_count(), initial_count);
}

struct Node {
    int value;
    void* next;
    char data[1024*256];
    Node(int val) : value(val) {
    }
};

TEST_F(GCBasicTest, BasicCyclicReference) {
    gc_collect(true);
    size_t initial_size = get_old_gen_size() + get_young_gen_size();

    void* node1_ptr = gc_malloc(sizeof(Node), true, nullptr);
    Node* node1 = new(node1_ptr) Node(1);

    void* node2_ptr = gc_malloc(sizeof(Node), false, node1_ptr);
    Node* node2 = new(node2_ptr) Node(2);

    void* node3_ptr = gc_malloc(sizeof(Node), false, node2_ptr);
    Node* node3 = new(node3_ptr) Node(3);

    change_parent(node1_ptr, node2_ptr);

    node1->next = node2_ptr;
    node2->next = node3_ptr;
    node3->next = node1_ptr;

    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_EQ(node1->value, 1);
    ASSERT_EQ(node2->value, 2);
    ASSERT_EQ(node3->value, 3);

    ASSERT_EQ(node1->next, node2_ptr);
    ASSERT_EQ(node2->next, node3_ptr);
    ASSERT_EQ(node3->next, node1_ptr);


    gc_free(node1_ptr);

    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_EQ(get_old_gen_size() + get_young_gen_size(), initial_size);
}

class MultithreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        configure_thresholds(YOUNG_THRESHOLD, OLD_THRESHOLD, YOUNG_RATIO, OLD_RATIO);
    }

    static void AllocationWorker(int objectCount,
                                 size_t objectSize,
                                 std::atomic<int>& completedTasks) {
        std::vector<void*> objects;

        for (int i = 0; i < objectCount; i++) {
            void* obj = gc_malloc(objectSize, (i % 5 == 0), nullptr);
            objects.push_back(obj);
            if (i > 0 && i % 3 == 0) {
                gc_malloc(objectSize / 2, false, objects[i-1]);
            }
        }
        for (size_t i = 0; i < objects.size(); i += 2) {
            gc_free(objects[i]);
        }
        completedTasks.fetch_add(1);
    }
};

const size_t THREAD_COUNT = std::thread::hardware_concurrency()-2;

TEST_F(MultithreadTest, BasicAllocation) {

    const int obj_in_thread = 100;
    const size_t obj_size = 1024; // 1 KB

    size_t initial_memory = get_old_gen_size() + get_young_gen_size();
    std::atomic<int> completedTasks(0);
    std::vector<std::thread> threads;


    for (int i = 0; i < THREAD_COUNT; i++) {
        threads.push_back(std::thread(AllocationWorker,
                                      obj_in_thread,
                                      obj_size,
                                      std::ref(completedTasks)));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_EQ(completedTasks.load(), THREAD_COUNT);
    ASSERT_GT(get_old_gen_size()+get_young_gen_size(), initial_memory);

    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_GT(get_old_gen_size()+get_young_gen_size(), initial_memory);
}

TEST_F(MultithreadTest, StressTest) {
    const int iterations_count = 5;

    size_t initial_collections = get_collections_count();

    for (int iter = 0; iter < iterations_count; iter++) {
        std::atomic<int> completedTasks(0);
        std::vector<std::thread> threads;
        for (int i = 0; i < THREAD_COUNT; i++) {
            int objectCount = 100;
            size_t objectSize = 512;

            threads.push_back(std::thread(AllocationWorker,
                                          objectCount,
                                          objectSize,
                                          std::ref(completedTasks)));
        }
        for (auto& thread : threads) {
            thread.join();
        }
        gc_collect(true);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ASSERT_GT(get_collections_count() - initial_collections, iterations_count);
}

TEST_F(MultithreadTest, ComplexGraphs) {
    std::vector<std::thread> threads;
    std::atomic<int> completedGraphs(0);

    size_t initial_young_gen = get_young_gen_size();
    size_t initial_old_gen = get_old_gen_size();

    auto createObjectGraph = [&completedGraphs](int graphSize) {
        std::vector<void*> roots;
        for (int i = 0; i < 5; i++) {
            roots.push_back(gc_malloc(64, true, nullptr));
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> rootDist(0, roots.size() - 1);

        std::vector<void*> allObjects = roots;

        for (int i = 0; i < graphSize; i++) {
            int parentIdx = gen() % allObjects.size();
            void* parent = allObjects[parentIdx];

            void* newObj = gc_malloc(32, false, parent);
            allObjects.push_back(newObj);

            if (i > 50 && i % 10 == 0) {
                int idxToFree = gen() % (allObjects.size() - 5);
                gc_free(allObjects[idxToFree]);
            }
        }

        for (size_t i = 0; i < roots.size(); i += 2) {
            gc_free(roots[i]);
        }

        completedGraphs.fetch_add(1);
    };

    for (int i = 0; i < THREAD_COUNT; i++) {
        int graphSize = 1000;
        threads.push_back(std::thread(createObjectGraph, graphSize));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(completedGraphs.load(), THREAD_COUNT);

    gc_collect(false);
    gc_collect(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ASSERT_LT(get_young_gen_size(), initial_young_gen + 1000*32);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
