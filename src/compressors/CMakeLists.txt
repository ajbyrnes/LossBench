# Compressors library

# ZLIB requirements
find_package(ZLIB REQUIRED)

# SZ3 requirements
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
find_package(SZ3 REQUIRED)

add_library(compressors STATIC
    Compressor.hpp
    ZlibCompressor.cpp
    ZlibCompressor.hpp
    SZ3Compressor.cpp
    SZ3Compressor.hpp
    factory.cpp
    factory.hpp
)

target_include_directories(
    compressors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    compressors PUBLIC
    ZLIB::ZLIB
    SZ3::SZ3 PkgConfig::ZSTD
)
