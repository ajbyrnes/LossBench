# Create target root-utils
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
