add_executable(GcTests
        test.cpp
)


add_executable(Benchmark benchmark.cpp)

target_link_libraries(GcTests
        GcCollector
        GTest::GTest
        GTest::Main
        benchmark::benchmark
)

target_link_libraries(Benchmark
        GcCollector
        GTest::GTest
        GTest::Main
        benchmark::benchmark
)

add_test(NAME GcTest COMMAND GcTests)
