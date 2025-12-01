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