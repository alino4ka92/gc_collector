add_executable(GcTests
        test.cpp
)

target_link_libraries(GcTests
        GcCollector
        GTest::GTest
        GTest::Main
)

add_test(NAME GcTest COMMAND GcTests)