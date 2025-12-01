# Compressors library

add_library(compressors STATIC
    Compressor.hpp
    ZlibCompressor.cpp
    ZlibCompressor.hpp
)

target_include_directories(
    compressors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    compressors PUBLIC
    utils
    ZLIB::ZLIB
)
