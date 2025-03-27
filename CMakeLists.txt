cmake_minimum_required(VERSION 3.14)
project(GcCollectorProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
        src/gc.cpp
        src/gc_impl.cpp
)

add_library(GcCollector STATIC ${SOURCES})

target_include_directories(GcCollector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

enable_testing()

add_subdirectory(tests)