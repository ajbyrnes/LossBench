# Target: root-utils library

find_package(
    ROOT REQUIRED COMPONENTS Core RIO Tree TreePlayer
)
include_directories(${ROOT_INCLUDE_DIRS})

add_library(root-utils STATIC
    root-utils.cpp
    root-utils.hpp
)

target_include_directories(
    root-utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    root-utils PUBLIC
    ROOT::Core ROOT::RIO ROOT::Tree ROOT::TreePlayer
)

# Silence the ROOT header #warning about a C++ standard mismatch; 
# the build intentionally uses C++20.
# Considered removing the C++20 dependency,
# but std::format from C++20 is quite useful,
# and ROOT will presumably move to requiring C++20 in the future.
target_compile_options(root-utils PRIVATE -Wno-cpp)
