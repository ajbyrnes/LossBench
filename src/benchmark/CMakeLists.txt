# Benchmark library
add_library(benchmark STATIC
    benchmark.hpp
    benchmark.cpp
)

target_include_directories(
    benchmark PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    benchmark PUBLIC
    compressors
)
