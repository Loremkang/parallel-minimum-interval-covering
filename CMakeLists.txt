cmake_minimum_required(VERSION 3.10)
project(ParallelMinimumIntervalCover CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Add ParlayLib
add_subdirectory(parlaylib EXCLUDE_FROM_ALL)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/parlaylib/include)

# Output binaries to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Test executables - Always use Debug mode
add_executable(test_interval_covering tests/test_interval_covering.cpp)
target_link_libraries(test_interval_covering parlay)
target_compile_options(test_interval_covering PRIVATE -g -O0)
target_compile_definitions(test_interval_covering PRIVATE DEBUG)

add_executable(test_sample_duplicates tests/test_sample_duplicates.cpp)
target_link_libraries(test_sample_duplicates parlay)
target_compile_options(test_sample_duplicates PRIVATE -g -O0)
target_compile_definitions(test_sample_duplicates PRIVATE DEBUG)

add_executable(test_simple_scan tests/test_simple_scan.cpp)
target_link_libraries(test_simple_scan parlay)
target_compile_options(test_simple_scan PRIVATE -g -O0)
target_compile_definitions(test_simple_scan PRIVATE DEBUG)

add_executable(debug_comparison tests/debug_comparison.cpp)
target_link_libraries(debug_comparison parlay)
target_compile_options(debug_comparison PRIVATE -g -O0)
target_compile_definitions(debug_comparison PRIVATE DEBUG)

add_executable(debug_scan_hang tests/debug_scan_hang.cpp)
target_link_libraries(debug_scan_hang parlay)
target_compile_options(debug_scan_hang PRIVATE -g -O0)
target_compile_definitions(debug_scan_hang PRIVATE DEBUG)

add_executable(test_generate_intervals tests/test_generate_intervals.cpp)
target_link_libraries(test_generate_intervals parlay)
target_compile_options(test_generate_intervals PRIVATE -g -O0)
target_compile_definitions(test_generate_intervals PRIVATE DEBUG)

# Optional: enable pthread
find_package(Threads REQUIRED)
target_link_libraries(test_interval_covering Threads::Threads)
target_link_libraries(test_sample_duplicates Threads::Threads)
target_link_libraries(test_simple_scan Threads::Threads)
target_link_libraries(debug_comparison Threads::Threads)
target_link_libraries(debug_scan_hang Threads::Threads)
target_link_libraries(test_generate_intervals Threads::Threads)

# Add custom target for running tests
add_custom_target(run_tests
    COMMAND test_interval_covering
    DEPENDS test_interval_covering
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Benchmark executables - Always use Release mode
if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/benchmark_interval_covering.cpp")
    add_executable(benchmark_interval_covering benchmarks/benchmark_interval_covering.cpp)
    target_link_libraries(benchmark_interval_covering parlay)
    target_link_libraries(benchmark_interval_covering Threads::Threads)
    target_compile_options(benchmark_interval_covering PRIVATE -O3 -march=native)
    target_compile_definitions(benchmark_interval_covering PRIVATE NDEBUG)

    add_custom_target(run_benchmark
        COMMAND benchmark_interval_covering
        DEPENDS benchmark_interval_covering
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/benchmark_thread_scaling.cpp")
    add_executable(benchmark_thread_scaling benchmarks/benchmark_thread_scaling.cpp)
    target_link_libraries(benchmark_thread_scaling parlay)
    target_link_libraries(benchmark_thread_scaling Threads::Threads)
    target_compile_options(benchmark_thread_scaling PRIVATE -O3 -march=native)
    target_compile_definitions(benchmark_thread_scaling PRIVATE NDEBUG)

    add_custom_target(run_thread_scaling
        COMMAND benchmark_thread_scaling
        DEPENDS benchmark_thread_scaling
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/benchmark_parallel_breakdown.cpp")
    add_executable(benchmark_parallel_breakdown benchmarks/benchmark_parallel_breakdown.cpp)
    target_link_libraries(benchmark_parallel_breakdown parlay)
    target_link_libraries(benchmark_parallel_breakdown Threads::Threads)
    target_compile_options(benchmark_parallel_breakdown PRIVATE -O3 -march=native)
    target_compile_definitions(benchmark_parallel_breakdown PRIVATE NDEBUG)

    add_custom_target(run_breakdown
        COMMAND benchmark_parallel_breakdown
        DEPENDS benchmark_parallel_breakdown
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

