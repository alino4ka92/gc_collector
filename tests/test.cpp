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
    EXPECT_EQ(*static_cast<int *>(ptr), 42);
    deallocate(ptr);
    gc_collect();
}

TEST_F(GenerationalGCTest, LinkedList) {
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


    EXPECT_EQ(*static_cast<int *>(root), 100);
    EXPECT_EQ(*static_cast<int *>(child), 101);
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

    gc_free(node1);

    gc_collect();

    void *new_obj = allocate(sizeof(TestNode), true, nullptr);

    gc_collect();
}



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
