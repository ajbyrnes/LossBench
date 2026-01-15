# GoogleTest discovery
include(GoogleTest)

# Zlib compressor tests
add_executable(test_zlib test_zlib.cpp)
target_include_directories(test_zlib PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_zlib PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_zlib)

# SZ3 compressor tests
add_executable(test_sz3 test_sz3.cpp)
target_include_directories(test_sz3 PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_sz3 PRIVATE compressors GTest::gtest_main)
gtest_discover_tests(test_sz3)

# Interface tests
add_executable(test_interface test_interface.cpp)
target_include_directories(test_interface PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_interface PRIVATE interface GTest::gtest_main)
gtest_discover_tests(test_interface)