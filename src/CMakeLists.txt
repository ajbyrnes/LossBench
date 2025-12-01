# Minimum version required by ROOT as of November 28, 2025
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Declare the project name and version
project(LossBench VERSION 1.0.0 LANGUAGES CXX)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the ROOT package
find_package(ROOT REQUIRED COMPONENTS RIO Tree)
include_directories(${ROOT_INCLUDE_DIRS})

# Main target
add_executable(benchmark benchmark.cpp)

target_link_libraries(
    benchmark PRIVATE
    ROOT::Core ROOT::RIO ROOT::Tree ROOT::TreePlayer
)

# Silence the ROOT header #warning about a C++ standard mismatch; 
# the build intentionally uses C++20.
# Considered removing the C++20 dependency,
# but std::format from C++20 is quite useful,
# and ROOT will presumably move to requiring C++20 in the future.
target_compile_options(benchmark PRIVATE -Wno-cpp)

# Enable testing
include(CTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) # harmless on Linux
FetchContent_MakeAvailable(googletest)
