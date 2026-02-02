# GoogleTest discovery
include(GoogleTest)

# Zstd compressor tests
add_executable(test_zstd test_zstd.cpp)
target_include_directories(test_zstd PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_zstd PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_zstd)

# Zstd-trunc compressor tests
add_executable(test_zstd_trunc test_zstd_trunc.cpp)
target_include_directories(test_zstd_trunc PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_zstd_trunc PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_zstd_trunc)

# SZ3 compressor tests (only if SZ3 is available)
if(HAS_SZ3)
    add_executable(test_sz3 test_sz3.cpp)
    target_include_directories(test_sz3 PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_sz3 PRIVATE compressors GTest::gtest_main)
    gtest_discover_tests(test_sz3)
endif()

# ZFP compressor tests (only if ZFP is available)
if(HAS_ZFP)
    add_executable(test_zfp test_zfp.cpp)
    target_include_directories(test_zfp PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_zfp PRIVATE compressors GTest::gtest_main)
    gtest_discover_tests(test_zfp)
endif()

# Interface tests
add_executable(test_interface test_interface.cpp)
target_include_directories(test_interface PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_interface PRIVATE interface GTest::gtest_main)
gtest_discover_tests(test_interface)
