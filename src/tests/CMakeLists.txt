# GoogleTest discovery
include(GoogleTest)

# Zlib compressor tests
add_executable(test_zlib test_zlib.cpp)
target_include_directories(test_zlib PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_zlib PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_zlib)

# Zlib-trunc compressor tests
add_executable(test_zlib_trunc test_zlib_trunc.cpp)
target_include_directories(test_zlib_trunc PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_zlib_trunc PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_zlib_trunc)

# SZ3 compressor tests (only if SZ3 is available)
if(HAS_SZ3)
    add_executable(test_sz3 test_sz3.cpp)
    target_include_directories(test_sz3 PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_sz3 PRIVATE compressors GTest::gtest_main)
    gtest_discover_tests(test_sz3)
endif()

# Interface tests
add_executable(test_interface test_interface.cpp)
target_include_directories(test_interface PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_interface PRIVATE interface GTest::gtest_main)
gtest_discover_tests(test_interface)