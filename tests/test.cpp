#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <algorithm>
#include "gc.h"

struct TestNode {
    int value;
    TestNode *next;
};

class GenerationalGCTest : public ::testing::Test {
protected:
    void SetUp() override {
        gc_collect();
        gc_collect();
        gc_collect();
    }

    void TearDown() override {
        while (allocated_memory.size() > 0) {
            void *ptr = *allocated_memory.begin();
            try {
                deallocate(ptr);
            } catch (...) {
            }
            allocated_memory.erase(ptr);
        }
        allocated_memory.clear();

        gc_collect();
        gc_collect();
        gc_collect();
    }

    void *allocate(size_t size, bool is_root, void *parent) {
        void *ptr = gc_malloc(size, is_root, parent);
        allocated_memory.insert(ptr);
        return ptr;
    }

    void deallocate(void *ptr) {
        gc_free(ptr);
    }

    std::set<void *> allocated_memory;
};

TEST_F(GenerationalGCTest, BasicAllocationAndFree) {
    void *ptr = allocate(sizeof(int), true, nullptr);
    ASSERT_NE(ptr, nullptr);
    *static_cast<int *>(ptr) = 42;
    EXPECT_EQ(*static_cast<int*>(ptr), 42);
    deallocate(ptr);
    gc_collect();
}

TEST_F(GenerationalGCTest, LinkedNodesNotCollected) {
    TestNode *root = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    root->value = 1;

    TestNode *current = root;
    for (int i = 2; i <= 5; i++) {
        TestNode *node = static_cast<TestNode *>(allocate(sizeof(TestNode), false, current));
        node->value = i;
        node->next = nullptr;
        current->next = node;
        current = node;
    }

    gc_collect();

    current = root;
    for (int i = 1; i <= 5; i++) {
        ASSERT_NE(current, nullptr);
        EXPECT_EQ(current->value, i);
        current = current->next;
    }
}

TEST_F(GenerationalGCTest, YoungGenerationCollection) {
    void *root = allocate(sizeof(int), true, nullptr);
    *static_cast<int *>(root) = 100;

    for (int i = 0; i < 10; i++) {
        void *object = allocate(sizeof(int), false, nullptr);
        *static_cast<int *>(object) = i;
    }

    gc_collect();

    void *child = allocate(sizeof(int), false, root);
    *static_cast<int *>(child) = 101;

    gc_collect();

    gc_collect();

    EXPECT_EQ(*static_cast<int*>(root), 100);
    EXPECT_EQ(*static_cast<int*>(child), 101);
}

TEST_F(GenerationalGCTest, ObjectPromotionToOldGeneration) {
    TestNode *root = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    root->value = 1;
    root->next = nullptr;

    TestNode *child = static_cast<TestNode *>(allocate(sizeof(TestNode), false, root));
    child->value = 2;
    child->next = nullptr;
    root->next = child;

    gc_collect();

    gc_collect();

    gc_collect();

    TestNode *grandchild = static_cast<TestNode *>(allocate(sizeof(TestNode), false, child));
    grandchild->value = 3;
    grandchild->next = nullptr;
    child->next = grandchild;

    gc_collect();

    EXPECT_EQ(root->value, 1);
    EXPECT_NE(root->next, nullptr);
    EXPECT_EQ(root->next->value, 2);
    EXPECT_NE(root->next->next, nullptr);
    EXPECT_EQ(root->next->next->value, 3);
}

TEST_F(GenerationalGCTest, RootRemovalAndCollectionCascade) {
    TestNode *root = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    root->value = 100;

    TestNode *child1 = static_cast<TestNode *>(allocate(sizeof(TestNode), false, root));
    child1->value = 101;

    TestNode *child2 = static_cast<TestNode *>(allocate(sizeof(TestNode), false, root));
    child2->value = 102;

    root->next = child1;
    child1->next = child2;
    child2->next = nullptr;

    gc_free(root);

    gc_collect();
    gc_collect();
    gc_collect();

    TestNode *new_root = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    new_root->value = 200;
    new_root->next = nullptr;

    EXPECT_EQ(new_root->value, 200);
}

TEST_F(GenerationalGCTest, CyclicReferences) {
    TestNode *node1 = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    TestNode *node2 = static_cast<TestNode *>(allocate(sizeof(TestNode), false, node1));
    TestNode *node3 = static_cast<TestNode *>(allocate(sizeof(TestNode), false, node2));

    node1->value = 1;
    node2->value = 2;
    node3->value = 3;

    node1->next = node2;
    node2->next = node3;
    node3->next = node1;

    gc_collect();

    gc_free(node1);

    gc_collect();
    gc_collect();
    gc_collect();

    void *new_obj = allocate(sizeof(TestNode), true, nullptr);
    ASSERT_NE(new_obj, nullptr);
}

TEST_F(GenerationalGCTest, StressTest) {
    const int NUM_OBJECTS = 1000;
    const int NUM_ROOTS = 10;

    std::vector<void *> roots;

    for (int i = 0; i < NUM_ROOTS; i++) {
        void *root = allocate(sizeof(int), true, nullptr);
        *static_cast<int *>(root) = i;
        roots.push_back(root);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_ROOTS - 1);

    for (int i = 0; i < NUM_OBJECTS; i++) {
        int root_idx = dis(gen);
        void *obj = allocate(sizeof(int), false, roots[root_idx]);
        *static_cast<int *>(obj) = NUM_ROOTS + i;
    }

    for (int i = 0; i < 3; i++) {
        gc_collect();
    }

    for (int i = 0; i < NUM_ROOTS / 2; i++) {
        gc_free(roots[i]);
        roots[i] = nullptr;
    }

    for (int i = 0; i < 3; i++) {
        gc_collect();
    }

    for (int i = NUM_ROOTS / 2; i < NUM_ROOTS; i++) {
        EXPECT_EQ(*static_cast<int*>(roots[i]), i);
    }
}

TEST_F(GenerationalGCTest, CollectionSequence) {
    for (int i = 0; i < 10; i++) {
        void *obj = allocate(sizeof(int), true, nullptr);
        *static_cast<int *>(obj) = i;
    }

    gc_collect();

    for (int i = 0; i < 5; i++) {
        void *temp = allocate(sizeof(int), false, nullptr);
        *static_cast<int *>(temp) = 100 + i;
    }

    gc_collect();
    gc_collect();
    gc_collect();
}

TEST_F(GenerationalGCTest, WriteBarrier) {
    TestNode *root = static_cast<TestNode *>(allocate(sizeof(TestNode), true, nullptr));
    root->value = 1;
    root->next = nullptr;

    gc_collect();
    gc_collect();
    gc_collect();

    TestNode *young_obj = static_cast<TestNode *>(allocate(sizeof(TestNode), false, root));
    young_obj->value = 2;
    young_obj->next = nullptr;
    root->next = young_obj;

    gc_collect();

    ASSERT_NE(root->next, nullptr);
    EXPECT_EQ(root->next->value, 2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
