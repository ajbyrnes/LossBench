# Create target utils
add_library(utils STATIC
    root.cpp
    root.hpp
    interface.cpp
    interface.hpp
)

target_include_directories(
    utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    utils PUBLIC
    ROOT::Core ROOT::RIO ROOT::Tree ROOT::TreePlayer
)