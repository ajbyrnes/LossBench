# Minimum version required by ROOT as of November 28, 2025
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Declare the project name and version
project(LossBench VERSION 1.0.0 LANGUAGES CXX)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Subdirectories
add_subdirectory(root-utils)
add_subdirectory(compressors)
add_subdirectory(benchmark)
add_subdirectory(interface)

# Main target
add_executable(lossbench lossbench.cpp)

target_link_libraries(
    lossbench PRIVATE
    compressors
    benchmark
    root-utils
    interface
)

# Enable testing
#include(CTest)
#include(FetchContent)
#FetchContent_Declare(
#  googletest
#  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
#)
#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # harmless on Linux
#FetchContent_MakeAvailable(googletest)
