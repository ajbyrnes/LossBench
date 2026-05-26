# Benchmark library
add_library(benchmark OBJECT
    benchmark.hpp
    benchmark.cpp
    throughput_metrics.cpp
    error_metrics.cpp
    distribution_metrics.cpp
)

target_include_directories(
    benchmark PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(nlohmann_json REQUIRED)

target_link_libraries(
    benchmark PUBLIC
    compressors
    nlohmann_json::nlohmann_json
)
