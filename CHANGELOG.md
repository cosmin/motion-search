# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed - Highway SIMD Migration

This release represents a complete modernization of the codebase, migrating from platform-specific SSE2 intrinsics to Google's Highway library for portable, cross-platform SIMD support.

#### Phase 1: Highway Integration
- Added Google Highway 1.2.0 as a dependency via CMake FetchContent
- Highway library is automatically downloaded and built during configuration
- Supports multiple SIMD instruction sets: SSE2, AVX2, AVX-512 (x86), NEON (ARM)

#### Phase 2: C to C++ Modernization
- Converted all C source files to C++ (`.c` → `.cpp`)
  - `motion_search/cpu.c` → `cpu.cpp`
  - `motion_search/frame.c` → `frame.cpp`
  - `motion_search/moments.c` → `moments.cpp`
  - `motion_search/motion_search.c` → `motion_search.cpp`
- Updated header files to properly handle C++ compilation
- Applied modern C++ casts (`static_cast`) where appropriate
- Maintained full API compatibility with existing code

#### Phase 3: SIMD Migration to Highway
- Created new Highway-based SIMD implementation: `motion_search/asm/moments.highway.cpp`
- Migrated all 12 SIMD functions to Highway's portable API:
  - `fastSAD16/8/4` - Sum of Absolute Differences
  - `fast_variance16/8/4` - Variance calculation
  - `fast_calc_mse16/8/4` - Mean Squared Error
  - `fast_bidir_mse16/8/4` - Bidirectional MSE
- Highway automatically selects the best available instruction set at runtime
- Simplified dispatch system in `moments.disp.cpp`

#### Phase 4: Legacy Code Cleanup
- Removed old SSE2 implementation: `motion_search/asm/moments.x86.sse2.c`
- Removed CPU detection code: `cpu.cpp`, `cpu.h`, `cpu_disp.h`
- Removed all platform-specific conditional compilation
- Simplified CMakeLists.txt to always use Highway (removed `USE_HIGHWAY_SIMD` option)
- Highway now handles all target-specific dispatch internally

#### Phase 5: Final Integration and Documentation
- Updated CI configuration to test only Highway implementation
  - Removed SSE2 testing matrix from GitHub Actions workflow
  - Streamlined build configuration
- Cleaned up header files
  - Removed orphaned SSE2 function declarations from `moments.h`
- Updated documentation
  - README.md now documents Highway dependency and requirements
  - Added comprehensive CHANGELOG.md
  - Updated optimization guide for Highway-based approach
- Removed SSE2 references from test file comments
- All tests passing (34/34 non-skipped tests)

### Added
- Google Highway 1.2.0 dependency for portable SIMD
- Comprehensive Highway-based SIMD implementation
- Cross-platform support: x86 (SSE2/AVX2/AVX-512), ARM (NEON)
- C++11 compiler requirement

### Removed
- SSE2-specific intrinsics and implementations
- Platform-specific CPUID detection code
- Conditional compilation for SSE2 vs other platforms
- `USE_HIGHWAY_SIMD` CMake option (Highway is now always used)

### Technical Details

**Before (SSE2-based):**
```
main functions → CPU dispatcher (CPUID detection)
                 ↓
                 ├─> SSE2 functions (x86 only)
                 └─> C reference functions (fallback)
```

**After (Highway-based):**
```
main functions → simple wrappers (moments.disp.cpp)
                 ↓
                 Highway multi-target dispatch (moments.highway.cpp)
                 ↓
                 → SSE2/AVX2/AVX-512/NEON (auto-selected by Highway)
```

**Benefits:**
- Cross-platform SIMD support (not just x86)
- Automatic runtime dispatch to best available instruction set
- Simpler codebase with less platform-specific code
- Better maintainability through Highway's portable API
- Future-proof for new SIMD instruction sets

### Migration Notes

This is a **breaking change** for build configuration:
- The `USE_HIGHWAY_SIMD` CMake option has been removed
- Highway is now the only SIMD backend and is always enabled
- Projects integrating this library should update their build scripts accordingly

**Compatibility:**
- API remains unchanged - all public functions have the same signatures
- Behavior is functionally equivalent to the SSE2 implementation
- Test results confirm pixel-perfect output matching

**Performance:**
- Highway's optimizations match or exceed hand-tuned SSE2 performance
- Additional performance gains on AVX2 and AVX-512 capable systems
- ARM platforms now benefit from NEON optimizations

## Historical Changes

See git commit history for changes prior to the Highway migration.
