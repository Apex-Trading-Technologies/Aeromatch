cmake_minimum_required(VERSION 3.20)
project(aeromatch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AEROMATCH_BUILD_TESTS "Build AeroMatch unit tests" ON)
option(AEROMATCH_BUILD_BENCHMARKS "Build AeroMatch benchmarks" ON)
option(AEROMATCH_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
    if(AEROMATCH_ENABLE_ASAN)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

include(FetchContent)

# -----------------------------------------------------------------------------
# Dependencies via FetchContent
# -----------------------------------------------------------------------------

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# GoogleTest
if(AEROMATCH_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Google Benchmark
if(AEROMATCH_BUILD_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

# -----------------------------------------------------------------------------
# AeroMatch Core Library
# -----------------------------------------------------------------------------
add_library(aeromatch_core STATIC
    src/core/order_book.cpp
    src/core/matching_engine.cpp
)

target_include_directories(aeromatch_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(aeromatch_core
    PUBLIC
        spdlog::spdlog
)

# -----------------------------------------------------------------------------
# AeroMatch Standalone Binary
# -----------------------------------------------------------------------------
add_executable(aeromatch src/main.cpp)
target_link_libraries(aeromatch PRIVATE aeromatch_core)

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
if(AEROMATCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(AEROMATCH_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
