# Compressors library

# ZLIB is required (core dependency)
find_package(ZLIB REQUIRED)

# SZ3 is optional
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZSTD QUIET IMPORTED_TARGET libzstd)
endif()
find_package(SZ3 QUIET)

# Base compressor sources (always included)
set(COMPRESSOR_SOURCES
    Compressor.hpp
    ZlibCompressor.cpp
    ZlibCompressor.hpp
    ZlibTruncCompressor.cpp
    ZlibTruncCompressor.hpp
    factory.cpp
    factory.hpp
)

# Base link libraries
set(COMPRESSOR_LIBS ZLIB::ZLIB)

# Add SZ3 if available
if(SZ3_FOUND AND ZSTD_FOUND)
    message(STATUS "SZ3 found - enabling SZ3 compressor")
    list(APPEND COMPRESSOR_SOURCES SZ3Compressor.cpp SZ3Compressor.hpp)
    list(APPEND COMPRESSOR_LIBS SZ3::SZ3 PkgConfig::ZSTD)
    set(HAS_SZ3 TRUE)
else()
    message(STATUS "SZ3 not found - SZ3 compressor disabled")
    set(HAS_SZ3 FALSE)
endif()

add_library(compressors STATIC ${COMPRESSOR_SOURCES})

target_include_directories(
    compressors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(compressors PUBLIC ${COMPRESSOR_LIBS})

# Export compile definitions for conditional compilation
if(HAS_SZ3)
    target_compile_definitions(compressors PUBLIC HAS_SZ3)
endif()

# Propagate to parent scope for tests
set(HAS_SZ3 ${HAS_SZ3} PARENT_SCOPE)
