# Compressors library

# zstd is required (core dependency)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)

# SZ3 is optional
find_package(SZ3 QUIET)

# ZFP is optional
find_package(zfp QUIET)

# SZp is optional
find_package(SZp QUIET)

# SPERR is optional (no CMake config, so we find it manually)
find_library(SPERR_LIBRARY NAMES SPERR)
find_path(SPERR_INCLUDE_DIR NAMES SPERR_C_API.h)

# Base compressor sources (always included)
set(COMPRESSOR_SOURCES
    Compressor.hpp
    ZstdCompressor.cpp
    ZstdCompressor.hpp
    ZstdTruncCompressor.cpp
    ZstdTruncCompressor.hpp
    UniformHistogramCompressor.cpp
    UniformHistogramCompressor.hpp
    QuantileHistogramCompressor.cpp
    QuantileHistogramCompressor.hpp
    QuantileResidualCompressor.cpp
    QuantileResidualCompressor.hpp
    factory.cpp
    factory.hpp
)

# Base link libraries
set(COMPRESSOR_LIBS PkgConfig::ZSTD)

# Add SZ3 if available
if(SZ3_FOUND)
    message(STATUS "SZ3 found - enabling SZ3 compressor")
    list(APPEND COMPRESSOR_SOURCES SZ3Compressor.cpp SZ3Compressor.hpp)
    list(APPEND COMPRESSOR_LIBS SZ3::SZ3)
    set(HAS_SZ3 TRUE)
else()
    message(STATUS "SZ3 not found - SZ3 compressor disabled")
    set(HAS_SZ3 FALSE)
endif()

# Add ZFP if available
if(zfp_FOUND)
    message(STATUS "ZFP found - enabling ZFP compressor")
    list(APPEND COMPRESSOR_SOURCES ZFPCompressor.cpp ZFPCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS zfp::zfp)
    set(HAS_ZFP TRUE)
else()
    message(STATUS "ZFP not found - ZFP compressor disabled")
    set(HAS_ZFP FALSE)
endif()

# Add SPERR if available
if(SPERR_LIBRARY AND SPERR_INCLUDE_DIR)
    message(STATUS "SPERR found - enabling SPERR compressor")
    message(STATUS "  Library: ${SPERR_LIBRARY}")
    message(STATUS "  Include: ${SPERR_INCLUDE_DIR}")
    list(APPEND COMPRESSOR_SOURCES SPERRCompressor.cpp SPERRCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS ${SPERR_LIBRARY})
    set(SPERR_FOUND TRUE)
    set(HAS_SPERR TRUE)
else()
    message(STATUS "SPERR not found - SPERR compressor disabled")
    set(SPERR_FOUND FALSE)
    set(HAS_SPERR FALSE)
endif()

# ZFP-X is optional (ZFP fork — found manually to avoid target conflict with ZFP)
find_library(ZFPX_LIBRARY NAMES zfp HINTS $ENV{HOME}/ZFP-X/build/lib NO_DEFAULT_PATH)
find_path(ZFPX_INCLUDE_DIR NAMES zfp.h HINTS $ENV{HOME}/ZFP-X/include NO_DEFAULT_PATH)

# MGARD is optional
find_package(mgard QUIET HINTS $ENV{HOME}/local/MGARD/build)

# Add SZp if available
if(SZp_FOUND)
    message(STATUS "SZp found - enabling SZp compressor")
    list(APPEND COMPRESSOR_SOURCES SZpCompressor.cpp SZpCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS SZp::SZp)
    set(HAS_SZP TRUE)
else()
    message(STATUS "SZp not found - SZp compressor disabled")
    set(HAS_SZP FALSE)
endif()

# Add ZFP-X if available
if(ZFPX_LIBRARY AND ZFPX_INCLUDE_DIR)
    message(STATUS "ZFP-X found - enabling ZFP-X compressor")
    message(STATUS "  Library: ${ZFPX_LIBRARY}")
    message(STATUS "  Include: ${ZFPX_INCLUDE_DIR}")
    list(APPEND COMPRESSOR_SOURCES ZFPXCompressor.cpp ZFPXCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS ${ZFPX_LIBRARY})
    set(ZFPX_FOUND TRUE)
    set(HAS_ZFPX TRUE)
else()
    message(STATUS "ZFP-X not found - ZFP-X compressor disabled")
    set(ZFPX_FOUND FALSE)
    set(HAS_ZFPX FALSE)
endif()


# Add MGARD if available
if(mgard_FOUND)
    message(STATUS "MGARD found - enabling MGARD compressor")
    list(APPEND COMPRESSOR_SOURCES MGARDCompressor.cpp MGARDCompressor.hpp)
    list(APPEND COMPRESSOR_LIBS mgard::mgard)
    set(HAS_MGARD TRUE)
else()
    message(STATUS "MGARD not found - MGARD compressor disabled")
    set(HAS_MGARD FALSE)
endif()

# TuckerMPI (serial) is optional
find_library(TUCKER_LIBRARY NAMES serial_tucker HINTS $ENV{HOME}/TuckerMPI/build/serial)
find_path(TUCKER_INCLUDE_DIR NAMES Tucker.hpp HINTS $ENV{HOME}/TuckerMPI/src/serial)

if(TUCKER_LIBRARY AND TUCKER_INCLUDE_DIR)
    find_package(BLAS QUIET)
    find_package(LAPACK QUIET)
    if(BLAS_FOUND AND LAPACK_FOUND)
        message(STATUS "TuckerMPI found - enabling Tucker compressor")
        message(STATUS "  Library: ${TUCKER_LIBRARY}")
        message(STATUS "  Include: ${TUCKER_INCLUDE_DIR}")
        list(APPEND COMPRESSOR_SOURCES TuckerCompressor.cpp TuckerCompressor.hpp)
        list(APPEND COMPRESSOR_LIBS ${TUCKER_LIBRARY} ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
        set(TUCKER_FOUND TRUE)
        set(HAS_TUCKER TRUE)
    else()
        message(STATUS "TuckerMPI found but missing BLAS/LAPACK - Tucker compressor disabled")
        set(TUCKER_FOUND FALSE)
        set(HAS_TUCKER FALSE)
    endif()
else()
    message(STATUS "TuckerMPI not found - Tucker compressor disabled")
    set(TUCKER_FOUND FALSE)
    set(HAS_TUCKER FALSE)
endif()

# ISABELA is optional (no CMake config, find manually)
file(GLOB _ISABELA_DIRS "$ENV{HOME}/ISABELA*/lib")
file(GLOB _ISABELA_INC_DIRS "$ENV{HOME}/ISABELA*/include")
find_library(ISABELA_LIBRARY NAMES isabela HINTS ${_ISABELA_DIRS})
find_path(ISABELA_INCLUDE_DIR NAMES isabela.h HINTS ${_ISABELA_INC_DIRS})

if(ISABELA_LIBRARY AND ISABELA_INCLUDE_DIR)
    # ISABELA depends on GSL and zlib
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GSL QUIET gsl)
    find_package(ZLIB QUIET)
    if(GSL_FOUND AND ZLIB_FOUND)
        message(STATUS "ISABELA found - enabling ISABELA compressor")
        message(STATUS "  Library: ${ISABELA_LIBRARY}")
        message(STATUS "  Include: ${ISABELA_INCLUDE_DIR}")
        list(APPEND COMPRESSOR_SOURCES ISABELACompressor.cpp ISABELACompressor.hpp)
        list(APPEND COMPRESSOR_LIBS ${ISABELA_LIBRARY} ${GSL_LIBRARIES} ZLIB::ZLIB)
        set(ISABELA_FOUND TRUE)
        set(HAS_ISABELA TRUE)
    else()
        message(STATUS "ISABELA found but missing GSL/zlib - ISABELA compressor disabled")
        set(ISABELA_FOUND FALSE)
        set(HAS_ISABELA FALSE)
    endif()
else()
    message(STATUS "ISABELA not found - ISABELA compressor disabled")
    set(ISABELA_FOUND FALSE)
    set(HAS_ISABELA FALSE)
endif()

add_library(compressors STATIC ${COMPRESSOR_SOURCES})

target_include_directories(
    compressors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add SPERR include directory if found
if(SPERR_FOUND)
    target_include_directories(compressors PUBLIC ${SPERR_INCLUDE_DIR})
endif()

# Add ZFP-X include directory if found
if(ZFPX_FOUND)
    target_include_directories(compressors PUBLIC ${ZFPX_INCLUDE_DIR})
endif()

# Add Tucker include directory if found
if(TUCKER_FOUND)
    target_include_directories(compressors PUBLIC ${TUCKER_INCLUDE_DIR})
endif()

# Add ISABELA include directory if found
if(ISABELA_FOUND)
    target_include_directories(compressors PUBLIC ${ISABELA_INCLUDE_DIR})
endif()

target_link_libraries(compressors PUBLIC ${COMPRESSOR_LIBS})

# Export compile definitions for conditional compilation
if(HAS_SZ3)
    target_compile_definitions(compressors PUBLIC HAS_SZ3)
endif()
if(HAS_ZFP)
    target_compile_definitions(compressors PUBLIC HAS_ZFP)
endif()
if(HAS_SPERR)
    target_compile_definitions(compressors PUBLIC HAS_SPERR)
endif()
if(HAS_SZP)
    target_compile_definitions(compressors PUBLIC HAS_SZP)
endif()
if(HAS_MGARD)
    target_compile_definitions(compressors PUBLIC HAS_MGARD)
endif()
if(HAS_ZFPX)
    target_compile_definitions(compressors PUBLIC HAS_ZFPX)
endif()
if(HAS_TUCKER)
    target_compile_definitions(compressors PUBLIC HAS_TUCKER)
endif()
if(HAS_ISABELA)
    target_compile_definitions(compressors PUBLIC HAS_ISABELA)
endif()

# Propagate to parent scope for tests
set(HAS_SZ3 ${HAS_SZ3} PARENT_SCOPE)
set(HAS_ZFP ${HAS_ZFP} PARENT_SCOPE)
set(HAS_SPERR ${HAS_SPERR} PARENT_SCOPE)
set(HAS_SZP ${HAS_SZP} PARENT_SCOPE)
set(HAS_MGARD ${HAS_MGARD} PARENT_SCOPE)
set(HAS_ZFPX ${HAS_ZFPX} PARENT_SCOPE)
set(HAS_TUCKER ${HAS_TUCKER} PARENT_SCOPE)
set(HAS_ISABELA ${HAS_ISABELA} PARENT_SCOPE)
