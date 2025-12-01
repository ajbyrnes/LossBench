# Interface library (CLI parsing + JSON output)
add_library(interface STATIC
    interface.cpp
    interface.hpp
)

find_package(nlohmann_json 3.2.0 REQUIRED)

target_include_directories(
    interface PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    interface PUBLIC
    benchmark
    nlohmann_json::nlohmann_json
)
