# Test suite for motion_search

include(GoogleTest)

# Test data directory
set(TEST_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test_data")

# Common test includes
include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/motion_search
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test: SIMD Primitives (moments functions)
add_executable(test_moments test_moments.cpp)
target_link_libraries(test_moments
  motion_search_lib
  motion_search_lib_simd
  GTest::gtest_main
)
target_compile_definitions(test_moments PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}")
gtest_discover_tests(test_moments)

# Test: Motion Search Algorithms
add_executable(test_motion_search test_motion_search.cpp)
target_link_libraries(test_motion_search
  motion_search_lib
  motion_search_lib_simd
  GTest::gtest_main
)
target_compile_definitions(test_motion_search PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}")
gtest_discover_tests(test_motion_search)

# Test: Frame Operations
add_executable(test_frame test_frame.cpp)
target_link_libraries(test_frame
  motion_search_lib
  motion_search_lib_simd
  GTest::gtest_main
)
target_compile_definitions(test_frame PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}")
gtest_discover_tests(test_frame)

# Test: End-to-end Integration
add_executable(test_integration test_integration.cpp)
target_link_libraries(test_integration
  motion_search_lib
  motion_search_lib_simd
  GTest::gtest_main
)
target_compile_definitions(test_integration PRIVATE TEST_DATA_DIR="${TEST_DATA_DIR}")
gtest_discover_tests(test_integration)
