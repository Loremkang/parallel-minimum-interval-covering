cmake_minimum_required(VERSION 3.10)
project(ParallelMinimumIntervalCover CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for better visibility of compilation flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable verbose output to show compilation commands
# Users can also use: cmake --build build --verbose
# or: make VERBOSE=1 / ninja -v
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Show full compilation commands")

# Add compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
# Note: Do not set CMAKE_CXX_FLAGS_RELEASE or CMAKE_CXX_FLAGS_DEBUG globally
# because we use per-target compilation options for precise control

# Report build type
# Note: Individual targets override global build type settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Global Build Type: Debug")
    message(STATUS "  (Note: Validation tests cover Debug and NDEBUG; benchmarks use Release)")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Global Build Type: Release")
    message(STATUS "  (Note: Validation tests cover Debug and NDEBUG; benchmarks use Release)")
else()
    message(STATUS "Global Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  (Note: Validation tests cover Debug and NDEBUG; benchmarks use Release)")
endif()

# Add ParlayLib
add_subdirectory(parlaylib EXCLUDE_FROM_ALL)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/parlaylib/include)

# Output binaries to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Test executables
add_executable(test_interval_covering tests/test_interval_covering.cpp)
target_link_libraries(test_interval_covering parlay)
target_compile_options(test_interval_covering PRIVATE -g -O0)
message(STATUS "test_interval_covering: Debug mode (-g -O0)")

add_executable(test_input_validation tests/test_input_validation.cpp)
target_link_libraries(test_input_validation parlay)
target_include_directories(test_input_validation PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
target_compile_options(test_input_validation PRIVATE -g -O0)
message(STATUS "test_input_validation: Debug mode (-g -O0)")

add_executable(test_input_validation_release tests/test_input_validation.cpp)
target_link_libraries(test_input_validation_release parlay)
target_include_directories(test_input_validation_release PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
target_compile_options(test_input_validation_release PRIVATE -O3)
target_compile_definitions(test_input_validation_release PRIVATE NDEBUG)
message(STATUS "test_input_validation_release: Release mode (-O3 -DNDEBUG)")

add_executable(test_generate_intervals benchmarks/test_generate_intervals.cpp)
target_link_libraries(test_generate_intervals parlay)
target_include_directories(test_generate_intervals PRIVATE ${CMAKE_SOURCE_DIR}/benchmarks)
target_compile_options(test_generate_intervals PRIVATE -g -O0)
message(STATUS "test_generate_intervals: Debug mode (-g -O0)")

# Optional: enable pthread
find_package(Threads REQUIRED)
target_link_libraries(test_interval_covering Threads::Threads)
target_link_libraries(test_input_validation Threads::Threads)
target_link_libraries(test_input_validation_release Threads::Threads)
target_link_libraries(test_generate_intervals Threads::Threads)

# Add custom target for running tests
add_custom_target(run_tests
    COMMAND test_interval_covering
    COMMAND test_generate_intervals
    COMMAND test_input_validation
    COMMAND test_input_validation_release
    DEPENDS test_interval_covering test_generate_intervals
            test_input_validation test_input_validation_release
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Benchmark executables - Always use Release mode
if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/benchmark_thread_scaling.cpp")
    add_executable(benchmark_thread_scaling benchmarks/benchmark_thread_scaling.cpp)
    target_link_libraries(benchmark_thread_scaling parlay)
    target_link_libraries(benchmark_thread_scaling Threads::Threads)
    target_compile_options(benchmark_thread_scaling PRIVATE -O3 -march=native)
    target_compile_definitions(benchmark_thread_scaling PRIVATE NDEBUG)
    message(STATUS "benchmark_thread_scaling: Release mode (-O3 -march=native -DNDEBUG)")

endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/benchmarks/benchmark_parallel_breakdown.cpp")
    add_executable(benchmark_parallel_breakdown benchmarks/benchmark_parallel_breakdown.cpp)
    target_link_libraries(benchmark_parallel_breakdown parlay)
    target_link_libraries(benchmark_parallel_breakdown Threads::Threads)
    target_compile_options(benchmark_parallel_breakdown PRIVATE -O3 -march=native)
    target_compile_definitions(benchmark_parallel_breakdown PRIVATE NDEBUG)
    message(STATUS "benchmark_parallel_breakdown: Release mode (-O3 -march=native -DNDEBUG)")

endif()

# Print compilation summary
message(STATUS "")
message(STATUS "=================================================================")
message(STATUS "                   COMPILATION OPTIONS SUMMARY")
message(STATUS "=================================================================")
message(STATUS "")
if(CMAKE_BUILD_TYPE)
    message(STATUS "Global CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  WARNING: CMAKE_BUILD_TYPE affects all targets!")
    message(STATUS "  For clean per-target flags, build without CMAKE_BUILD_TYPE:")
    message(STATUS "    cmake .. (without -DCMAKE_BUILD_TYPE=...)")
else()
    message(STATUS "Global CMAKE_BUILD_TYPE: Not set (recommended)")
    message(STATUS "  Each target uses its own optimized compilation flags")
endif()
message(STATUS "")
message(STATUS "Test Executables:")
message(STATUS "  - Default test flags: -g -O0")
message(STATUS "  - Validation is additionally tested with -O3 -DNDEBUG")
message(STATUS "  - Targets:")
message(STATUS "    • test_interval_covering")
message(STATUS "    • test_input_validation")
message(STATUS "    • test_input_validation_release")
message(STATUS "    • test_generate_intervals")
message(STATUS "")
message(STATUS "Benchmark Executables (Always Release mode):")
message(STATUS "  - Compilation flags: -O3 -march=native -DNDEBUG")
message(STATUS "  - Targets:")
message(STATUS "    • benchmark_thread_scaling")
message(STATUS "    • benchmark_parallel_breakdown")
message(STATUS "")
message(STATUS "To see detailed compilation commands during build:")
message(STATUS "  • ninja -v")
message(STATUS "  • make VERBOSE=1")
message(STATUS "  • Or check: ${CMAKE_BINARY_DIR}/compile_commands.json")
message(STATUS "=================================================================")
message(STATUS "")
