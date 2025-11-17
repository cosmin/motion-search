cmake_minimum_required(VERSION 3.10)
project (motion_search VERSION 0.0.1 DESCRIPTION "compute motion search for video complexity estimation")
set (CMAKE_CXX_STANDARD 17)
include(GNUInstallDirs)

# Testing support
option(BUILD_TESTING "Build the testing tree" ON)
include(CTest)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(ClangFormat)

# SIMD support option
option(USE_HIGHWAY_SIMD "Use Highway library for SIMD operations (recommended). If OFF, uses pure C implementations." ON)

# Abseil library for command-line parsing
include(FetchContent)

# Highway SIMD library (optional, enabled by default)
if(USE_HIGHWAY_SIMD)
  message(STATUS "Building with Highway SIMD support (USE_HIGHWAY_SIMD=ON)")
  FetchContent_Declare(
    highway
    GIT_REPOSITORY https://github.com/google/highway.git
    GIT_TAG 1.2.0
  )
  set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(highway)
else()
  message(STATUS "Building with pure C implementations (USE_HIGHWAY_SIMD=OFF)")
endif()

# Abseil for command-line parsing
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20240722.0  # LTS branch
)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(abseil)

# nlohmann/json for JSON output (Phase 2)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# tinyxml2 for XML output (Phase 2)
FetchContent_Declare(
  tinyxml2
  GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
  GIT_TAG 10.0.0
)
FetchContent_MakeAvailable(tinyxml2)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-fms-extensions)

# Create library : libmotion_search_lib
add_library(motion_search_lib
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
    "motion_search/OutputWriter.cpp"
    "motion_search/CSVWriter.cpp"
    "motion_search/JSONWriter.cpp"
    "motion_search/XMLWriter.cpp"
    "motion_search/DataConverter.cpp"
)
target_clangformat_setup(motion_search_lib)

# Link output writer dependencies
target_link_libraries(motion_search_lib PUBLIC nlohmann_json::nlohmann_json tinyxml2)

#
# Create library : libmotion_search_lib_simd (conditionally uses Highway or pure C)
#

if(USE_HIGHWAY_SIMD)
  add_library(motion_search_lib_simd "motion_search/asm/moments.highway.cpp")
  target_link_libraries(motion_search_lib_simd PUBLIC hwy)
  target_clangformat_setup(motion_search_lib_simd)
  target_compile_definitions(motion_search_lib PRIVATE USE_HIGHWAY_SIMD=1)
  target_compile_definitions(motion_search_lib_simd PRIVATE USE_HIGHWAY_SIMD=1)
endif()

#
# create binary
#

add_executable(motion_search
    "motion_search/main.cpp"
)
target_clangformat_setup(motion_search)


if(USE_HIGHWAY_SIMD)
  target_link_libraries (motion_search LINK_PUBLIC
      motion_search_lib
      motion_search_lib_simd
      absl::flags
      absl::flags_parse
      absl::flags_usage)
else()
  target_link_libraries (motion_search LINK_PUBLIC
      motion_search_lib
      absl::flags
      absl::flags_parse
      absl::flags_usage)
endif()

set_target_properties(motion_search_lib PROPERTIES PUBLIC_HEADER motion_search/motion_search.h)

target_include_directories(motion_search_lib PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/motion_search
)
if(USE_HIGHWAY_SIMD)
  target_include_directories(motion_search_lib_simd PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/motion_search
  )
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
if(USE_HIGHWAY_SIMD)
  install(TARGETS motion_search_lib_simd
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

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
