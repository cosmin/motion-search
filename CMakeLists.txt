cmake_minimum_required(VERSION 3.10)
project (motion_search VERSION 0.0.1 DESCRIPTION "compute motion search for video complexity estimation")
set (CMAKE_CXX_STANDARD 17)
include(GNUInstallDirs)

# Testing support
option(BUILD_TESTING "Build the testing tree" ON)
include(CTest)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ClangFormat)

# Highway SIMD library
option(USE_HIGHWAY_SIMD "Use Highway library for SIMD operations instead of SSE2" ON)
if(USE_HIGHWAY_SIMD)
  include(FetchContent)
  FetchContent_Declare(
    highway
    GIT_REPOSITORY https://github.com/google/highway.git
    GIT_TAG 1.2.0
  )
  set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(highway)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-fms-extensions)

# Create library : libmotion_search_lib
add_library(motion_search_lib
    "motion_search/cpu.cpp"
    "motion_search/frame.cpp"
    "motion_search/memory.cpp"
    "motion_search/moments.disp.cpp"
    "motion_search/BaseVideoSequenceReader.cpp"
    "motion_search/Y4MSequenceReader.cpp"
    "motion_search/YUVSequenceReader.cpp"
    "motion_search/ComplexityAnalyzer.cpp"
    "motion_search/EOFException.cpp"
    "motion_search/MotionVectorField.cpp"
    "motion_search/YUVFrame.cpp"
    "motion_search/moments.cpp"
    "motion_search/motion_search.cpp"
)
target_clangformat_setup(motion_search_lib)

#
# Create library : libmotion_search_lib_sse2 or libmotion_search_lib_highway
#

if(USE_HIGHWAY_SIMD)
  # Use Highway SIMD implementation
  set(SOURCES_SIMD "motion_search/asm/moments.highway.cpp")
  add_library(motion_search_lib_simd ${SOURCES_SIMD})
  target_link_libraries(motion_search_lib_simd PUBLIC hwy)
  target_compile_definitions(motion_search_lib_simd PUBLIC USE_HIGHWAY_SIMD)
  target_clangformat_setup(motion_search_lib_simd)
else()
  # Use SSE2 implementation
  set(SOURCES_SSE2 "motion_search/asm/moments.x86.sse2.c")
  set_source_files_properties(${SOURCES_SSE2} COMPILE_FLAGS "-msse2")
  add_library(motion_search_lib_simd ${SOURCES_SSE2})
  target_clangformat_setup(motion_search_lib_simd)
endif()

#
# create binary
#

add_executable(motion_search
    "motion_search/main.cpp"
)
target_clangformat_setup(motion_search)


target_link_libraries (motion_search LINK_PUBLIC
    motion_search_lib
    motion_search_lib_simd)

set_target_properties(motion_search_lib PROPERTIES PUBLIC_HEADER motion_search/motion_search.h)

target_include_directories(motion_search_lib PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/motion_search
)
target_include_directories(motion_search_lib_simd PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/motion_search
)
if(USE_HIGHWAY_SIMD)
  target_compile_definitions(motion_search_lib PUBLIC USE_HIGHWAY_SIMD)
endif()
target_include_directories(motion_search PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/motion_search
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fpermissive -Wall -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions -fpermissive -Wall -Wpedantic")

# Add -mxsave only on x86/x64 platforms where it's supported
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mxsave")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mxsave")
endif()

configure_file(motion_search.pc.in motion_search.pc @ONLY)

# build settings :  make release, make debug

ADD_CUSTOM_TARGET(debug
  COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
  COMMENT "Creating the executable in the debug mode.")

ADD_CUSTOM_TARGET(release
  COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
  COMMENT "Creating the executable in the release mode.")

install(TARGETS motion_search_lib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS motion_search_lib_simd
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${CMAKE_BINARY_DIR}/motion_search.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Testing
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_subdirectory(tests)
endif()
