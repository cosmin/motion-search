# Claude Code Development Guide

This guide explains how to effectively work with the motion-search project using Claude Code (or other AI coding assistants). It provides context about the codebase structure, development workflows, testing procedures, and common tasks.

---

## Project Overview

**motion-search** is a video complexity analyzer that estimates encoding difficulty using motion estimation, spatial analysis, and temporal prediction. It simulates an encoder's macroblock decision process (I/P/B frames) to predict compression complexity.

### Key Features
- **Motion Estimation:** PMVFAST algorithm with SIMD-optimized primitives
- **GOP Simulation:** Configurable I/P/B frame structures
- **Complexity Metrics:** Spatial variance, motion vectors, residual energy, MSE
- **Modern Codebase:** C++17 with Google Highway for portable SIMD

### Recent Major Changes
The project recently completed a 5-phase Highway migration:
1. Added Highway dependency
2. Modernized C → C++17
3. Migrated SSE2 intrinsics → Highway
4. Removed legacy dispatch code
5. Updated documentation

**Current Status:** Stable, all tests passing, ready for feature development

---

## Codebase Structure

```
motion-search/
├── motion_search/          # Core source code
│   ├── main.cpp            # Entry point, CLI parsing
│   ├── ComplexityAnalyzer.{h,cpp}     # Analysis orchestration
│   ├── motion_search.{h,cpp}          # PMVFAST algorithm
│   ├── MotionVectorField.{h,cpp}      # MV storage & prediction
│   ├── Y4MSequenceReader.{h,cpp}      # Y4M input parsing
│   ├── YUVSequenceReader.{h,cpp}      # Raw YUV input
│   ├── moments.{h,cpp,disp.cpp}       # SIMD primitive declarations
│   ├── asm/
│   │   └── moments.highway.cpp        # Highway SIMD implementations
│   ├── common.h            # Constants, configurations
│   └── fb_command_line_parser.h       # CLI parser (lightweight)
├── tests/                  # Google Test suite
│   ├── integration/        # End-to-end tests
│   ├── unit/              # Component tests
│   └── test_videos/       # Sample videos for testing
├── docs/                   # Documentation
│   └── OPTIMIZATION_GUIDE.md
├── cmake/                  # Build helpers
├── CMakeLists.txt         # Build configuration
├── FUTURE_IMPROVEMENTS_PLAN.md  # Roadmap (this plan)
└── CLAUDE.md              # This file
```

### Key Files for Development

| File | Purpose | When to Modify |
|------|---------|----------------|
| `main.cpp` | CLI parsing, program entry | Adding new flags, I/O handling |
| `ComplexityAnalyzer.{h,cpp}` | High-level analysis logic | New complexity metrics, workflow changes |
| `motion_search.{h,cpp}` | Core motion estimation | Algorithm improvements, search patterns |
| `moments.highway.cpp` | SIMD kernels | Performance optimization, new primitives |
| `common.h` | Constants (block size, search range) | Configuration changes |
| `CMakeLists.txt` | Build system | New dependencies, compiler flags |

---

## Development Environment

### Prerequisites
- **Compiler:** GCC 9+ or Clang 10+ (C++17 support)
- **CMake:** 3.10 or higher
- **Git:** For version control

### Dependencies (Auto-Fetched)
- **Google Highway** 1.2.0 - SIMD library
- **Google Test** 1.14.0 - Testing framework

### Build Instructions

```bash
# Clone repository (if not already)
git clone <repository-url>
cd motion-search

# Create build directory
mkdir -p build && cd build

# Configure (default: Release with SIMD)
cmake ..

# Or with options
cmake -DUSE_HIGHWAY_SIMD=ON -DBUILD_TESTING=ON ..

# Build
make -j$(nproc)

# Run tests
make test
# Or directly: ./bin/motion_search_tests

# Run program
./bin/motion_search -W=352 -H=288 -n=10 ../tests/test_videos/foreman_cif.y4m
```

### Build Targets
- `make` - Build everything (lib + executable + tests)
- `make motion_search` - Build only the main executable
- `make motion_search_tests` - Build only the test suite
- `make test` - Run all tests (requires prior build)

### Build Configurations
- **Debug:** `cmake -DCMAKE_BUILD_TYPE=Debug ..`
- **Release:** `cmake -DCMAKE_BUILD_TYPE=Release ..` (default)
- **Without SIMD:** `cmake -DUSE_HIGHWAY_SIMD=OFF ..`
- **Without Tests:** `cmake -DBUILD_TESTING=OFF ..`

---

## Testing Strategy

### Test Suite Organization

```
tests/
├── integration/
│   ├── MotionSearchIntegrationTest.cpp  # End-to-end analysis
│   └── HarnessTest.cpp                  # CLI and I/O
├── unit/
│   ├── MotionSearchTest.cpp             # Motion estimation logic
│   ├── MomentsTest.cpp                  # SIMD primitives
│   ├── Y4MReaderTest.cpp                # Input parsing
│   └── MotionVectorFieldTest.cpp        # MV prediction
└── test_videos/
    ├── generate_test_videos.sh          # Video generation script
    └── *.y4m                            # Sample videos
```

### Running Tests

```bash
# All tests
cd build
make test

# Or with verbose output
./bin/motion_search_tests

# Run specific test suite
./bin/motion_search_tests --gtest_filter="MotionSearchTest.*"

# Run single test
./bin/motion_search_tests --gtest_filter="MotionSearchTest.BasicMotionDetection"

# List all tests
./bin/motion_search_tests --gtest_list_tests
```

### Test Coverage Areas
- ✅ SIMD primitives (SAD, variance, MSE) - validated against C reference
- ✅ Motion vector prediction (median, zero, collocated)
- ✅ Y4M parsing (header extraction, frame reading)
- ✅ GOP structure (I/P/B frame assignment)
- ✅ End-to-end analysis (full video processing)
- ✅ Edge cases (1-frame videos, invalid inputs)

### Writing New Tests

```cpp
// Unit test example
TEST(MyFeatureTest, BasicFunctionality) {
  // Setup
  MyFeature feature(param1, param2);

  // Exercise
  auto result = feature.compute(input);

  // Verify
  EXPECT_EQ(result.value, expected_value);
  EXPECT_NEAR(result.score, 0.85, 1e-6);
}

// Integration test example
TEST(MyFeatureIntegrationTest, EndToEnd) {
  // Use test video
  std::string test_file = "../tests/test_videos/foreman_cif.y4m";

  // Run analysis
  ComplexityAnalyzer analyzer(test_file, config);
  auto results = analyzer.analyze();

  // Validate output
  ASSERT_GT(results.size(), 0);
  EXPECT_EQ(results[0].frame_type, FrameType::I);
}
```

---

## Common Development Tasks

### Task 1: Adding a New Command-Line Flag

**Current System:** `fb_command_line_parser.h` (custom, simple)
**Future System:** Abseil Flags (see FUTURE_IMPROVEMENTS_PLAN.md Phase 1)

**Current Approach:**
```cpp
// In main.cpp
int main(int argc, char** argv) {
  fb_command_line_parser::option_map options;

  // Add flag definition
  options["my_flag"] = { "42", "Description of my flag" };

  // Parse
  fb_command_line_parser::parse_cmd(argc, argv, options);

  // Use value
  int my_value = atoi(options["my_flag"].value.c_str());
}
```

**Testing:**
```bash
./motion_search -my_flag=100 input.y4m
```

### Task 2: Adding a New SIMD Primitive

**Files to modify:**
1. `motion_search/moments.h` - Declare function signature
2. `motion_search/asm/moments.highway.cpp` - Implement with Highway
3. `tests/unit/MomentsTest.cpp` - Add test case

**Example:**
```cpp
// 1. moments.h
int64_t fast_sum_squared_diff_16(const uint8_t* a, const uint8_t* b, int stride_a, int stride_b);

// 2. moments.highway.cpp
#include "hwy/highway.h"
namespace HWY_NAMESPACE {

int64_t fast_sum_squared_diff_16(const uint8_t* a, const uint8_t* b, int stride_a, int stride_b) {
  namespace hn = hwy::HWY_NAMESPACE;
  using V = hn::Vec<hn::ScalableTag<uint8_t>>;

  // Highway implementation
  // ... SIMD code here ...
}

}  // namespace HWY_NAMESPACE

// 3. MomentsTest.cpp
TEST(MomentsTest, SumSquaredDiff) {
  uint8_t block_a[16*16], block_b[16*16];
  // ... initialize test data ...

  int64_t result = fast_sum_squared_diff_16(block_a, block_b, 16, 16);
  EXPECT_GT(result, 0);
}
```

### Task 3: Adding a New Complexity Metric

**Files to modify:**
1. Create data structure in `ComplexityAnalyzer.h`
2. Implement computation in `ComplexityAnalyzer.cpp`
3. Update output in `main.cpp`
4. Add tests in `tests/integration/`

**Example:**
```cpp
// 1. ComplexityAnalyzer.h
struct FrameComplexity {
  int frame_num;
  // ... existing fields ...
  double my_new_metric;  // Add here
};

// 2. ComplexityAnalyzer.cpp
FrameComplexity ComplexityAnalyzer::analyzeFrame(const yuv_t& frame) {
  FrameComplexity result;
  // ... existing analysis ...

  // Compute new metric
  result.my_new_metric = computeMyMetric(frame);

  return result;
}

double ComplexityAnalyzer::computeMyMetric(const yuv_t& frame) {
  // Implementation
  return 0.0;
}

// 3. main.cpp - update CSV output
std::cout << result.frame_num << ","
          << result.frame_type << ","
          // ... existing fields ...
          << result.my_new_metric << "\n";

// 4. Test
TEST(ComplexityAnalyzerTest, NewMetric) {
  // Validate metric computation
}
```

### Task 4: Adding Support for a New Input Format

**Files to create/modify:**
1. Create `NewFormatReader.{h,cpp}` inheriting from `SequenceReader`
2. Update factory logic in `main.cpp`
3. Add CMake dependency if needed
4. Add tests in `tests/unit/`

**Example:**
```cpp
// 1. NewFormatReader.h
class NewFormatReader : public SequenceReader {
public:
  NewFormatReader(const std::string& filepath);
  ~NewFormatReader() override;

  bool readFrame(yuv_t* yuv_data) override;
  int getWidth() const override;
  int getHeight() const override;

private:
  // Format-specific state
};

// 2. main.cpp
std::unique_ptr<SequenceReader> createReader(const std::string& path) {
  if (path.ends_with(".newformat")) {
    return std::make_unique<NewFormatReader>(path);
  }
  // ... existing formats ...
}

// 3. CMakeLists.txt (if external library needed)
find_package(NewFormatLib REQUIRED)
target_link_libraries(motion_search_lib PRIVATE NewFormatLib::NewFormatLib)

// 4. Test
TEST(NewFormatReaderTest, ReadFrame) {
  NewFormatReader reader("test.newformat");
  yuv_t frame;
  ASSERT_TRUE(reader.readFrame(&frame));
}
```

### Task 5: Debugging Performance Issues

**Tools and Techniques:**

1. **Built-in Timing:**
```cpp
// Already in main.cpp
auto start = std::chrono::high_resolution_clock::now();
// ... analysis ...
auto end = std::chrono::high_resolution_clock::now();
std::cerr << "Time: "
          << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
          << " ms\n";
```

2. **Profiling with perf:**
```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make

# Profile
perf record ./bin/motion_search input.y4m
perf report
```

3. **Valgrind for Memory:**
```bash
valgrind --tool=callgrind ./bin/motion_search input.y4m
kcachegrind callgrind.out.*
```

4. **SIMD Verification:**
```cpp
// Check which Highway target is compiled
#include "hwy/highway.h"
std::cerr << "SIMD target: " << HWY_TARGET << "\n";
// 0 = AVX-512, 1 = AVX2, 2 = SSE4, etc.
```

---

## Code Style and Conventions

### General Principles
- **C++17 standard** - Use modern features (auto, range-for, structured bindings)
- **RAII** - Manage resources with constructors/destructors
- **Const correctness** - Mark functions and variables const where applicable
- **Avoid raw pointers** - Prefer references, smart pointers, or containers

### Formatting
The project uses **clang-format** for consistent style.

```bash
# Format single file
clang-format -i motion_search/main.cpp

# Format all source files
find motion_search tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

**Style highlights:**
- Indentation: 2 spaces (no tabs)
- Brace style: Allman (braces on new line)
- Naming:
  - Classes: `PascalCase` (e.g., `ComplexityAnalyzer`)
  - Functions: `camelCase` (e.g., `computeVariance`)
  - Variables: `snake_case` (e.g., `frame_count`)
  - Constants: `UPPER_CASE` (e.g., `MAX_SEARCH_RANGE`)
  - Private members: trailing underscore (e.g., `width_`)

### Comments
- **Header files:** Doxygen-style comments for public APIs
- **Implementation:** Explain "why" not "what"
- **Complex algorithms:** Reference papers or explain approach

```cpp
/// Computes motion vectors using PMVFAST algorithm.
///
/// @param ref Reference frame for motion search
/// @param cur Current frame to encode
/// @param search_range Maximum displacement in pixels
/// @return Motion vector field with per-block MVs
MotionVectorField computeMotionVectors(
    const yuv_t& ref,
    const yuv_t& cur,
    int search_range);
```

---

## Working with Git and Branches

### Branch Strategy
- `main` - Stable releases
- `claude/feature-name-<session-id>` - Development branches (auto-created)

### Commit Messages
Follow conventional commit style:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `refactor` - Code restructuring
- `perf` - Performance improvement
- `test` - Test additions/changes
- `docs` - Documentation
- `build` - Build system changes

**Examples:**
```
feat(cli): Add --output-format flag for JSON/XML

Implements JSON and XML output writers in addition to CSV.
Adds nlohmann/json dependency via CMake FetchContent.

Closes #42
```

```
perf(simd): Optimize SAD computation with Highway

Replaces scalar loop with vectorized Highway implementation.
Measured 3.2x speedup on AVX2 (see benchmark results).
```

### Pull Request Workflow

1. **Create branch:**
```bash
git checkout -b claude/my-feature-<session-id>
```

2. **Make changes, commit:**
```bash
git add .
git commit -m "feat: Add my feature"
```

3. **Push to remote:**
```bash
git push -u origin claude/my-feature-<session-id>
```

4. **Create PR** (via GitHub UI or `gh` CLI if available)

5. **Address review comments, push updates:**
```bash
git add .
git commit -m "fix: Address review comments"
git push
```

---

## Helpful Context for AI Assistants

### What to Know
1. **Recent refactor:** SSE2 → Highway (2025). Don't suggest SSE2 intrinsics.
2. **Testing is critical:** All new code needs tests (unit + integration).
3. **Performance matters:** This is a video analysis tool, optimize hot paths.
4. **Dependencies are minimal:** Only Highway and GTest. New deps need justification.
5. **Cross-platform:** Code should work on Linux, macOS, Windows (via MSVC/MinGW).

### Common Pitfalls
- ❌ Don't hardcode paths - use relative paths or command-line arguments
- ❌ Don't add platform-specific code without `#ifdef` guards
- ❌ Don't use SSE2/AVX intrinsics directly - use Highway abstractions
- ❌ Don't modify `common.h` constants without considering impact on tests
- ❌ Don't add heavy dependencies (OpenCV, Boost) - keep it lightweight

### Helpful Phrases for AI Context
When asking an AI assistant for help:

- "This codebase uses Google Highway for SIMD, not raw intrinsics"
- "Motion estimation uses PMVFAST algorithm, see `motion_search.cpp`"
- "Tests use Google Test framework, see `tests/` directory"
- "We recently migrated from C to C++17, prefer modern C++ idioms"
- "All SIMD primitives are in `moments.highway.cpp`, tested in `MomentsTest.cpp`"

---

## Debugging Tips

### Common Issues

**Issue:** `undefined reference to fast_*` functions
**Solution:** Make sure SIMD library is linked. Check `CMakeLists.txt` includes `motion_search_lib_simd`.

**Issue:** Test videos not found
**Solution:** Run tests from `build/` directory or use absolute paths.

**Issue:** Y4M parsing fails
**Solution:** Verify file has `YUV4MPEG2` signature. Try with sample videos in `tests/test_videos/`.

**Issue:** Performance slower than expected
**Solution:** Build in Release mode (`-DCMAKE_BUILD_TYPE=Release`). Check `HWY_TARGET` to verify SIMD level.

**Issue:** SIMD crashes (segfault, illegal instruction)
**Solution:**
- Check alignment (Highway handles this, but verify input pointers)
- Ensure CPU supports compiled target (e.g., AVX2)
- Use `HWY_DYNAMIC_DISPATCH` for runtime selection

### Logging and Diagnostics

**Add debug output:**
```cpp
#include <iostream>

// Temporary debug print
std::cerr << "DEBUG: frame_num=" << frame_num
          << " mv=(" << mv.x << "," << mv.y << ")\n";
```

**Conditional compilation:**
```cpp
#ifdef DEBUG_MOTION_SEARCH
  std::cerr << "Motion search iteration " << iter << "\n";
#endif
```

Build with:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DDEBUG_MOTION_SEARCH=ON ..
```

---

## Resources and References

### Documentation
- **Main README:** `README.md` - Project overview and usage
- **Optimization Guide:** `docs/OPTIMIZATION_GUIDE.md` - Performance tuning
- **Future Plan:** `FUTURE_IMPROVEMENTS_PLAN.md` - Roadmap for upcoming features

### External Resources
- **Google Highway:** https://github.com/google/highway - SIMD library docs
- **Google Test:** https://github.com/google/googletest - Testing framework
- **PMVFAST Paper:** "Fast Motion Estimation Algorithm" (Tourapis et al.)
- **H.264 Reference:** ITU-T Rec. H.264 - For understanding motion estimation context

### Video Codec Primers
If you're unfamiliar with video encoding concepts:
- **Macroblocks:** 16x16 pixel blocks (basic unit of encoding)
- **I/P/B Frames:** Intra (keyframe), Predicted (forward ref), Bidirectional (fwd+bwd ref)
- **Motion Vectors:** Displacement from reference frame (x, y offset)
- **GOP (Group of Pictures):** Sequence between I-frames
- **SAD/MSE:** Sum of Absolute Differences / Mean Squared Error (distortion metrics)

---

## Quick Reference

### Build Commands
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
make test
```

### Run Analysis
```bash
./bin/motion_search -W=1920 -H=1080 -n=100 -g=60 input.y4m > output.csv
```

### Run Specific Test
```bash
./bin/motion_search_tests --gtest_filter="MotionSearchTest.*"
```

### Format Code
```bash
find motion_search tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Profile Performance
```bash
perf record -g ./bin/motion_search input.y4m
perf report
```

---

## Future Development Roadmap

See `FUTURE_IMPROVEMENTS_PLAN.md` for comprehensive details. Summary:

1. **Phase 1:** Abseil command-line parsing (modern CLI)
2. **Phase 2:** JSON/XML/CSV output formats (flexible data export)
3. **Phase 3:** Optional FFmpeg input (support MP4, MKV, etc.)
4. **Phase 4:** Unified complexity score (learned metric combining factors)
5. **Phase 5:** DCT-based spatial complexity (advanced analysis, TBD)

Each phase is designed to be independently valuable and incrementally deliverable.

---

## Getting Help

### For Development Questions
1. Check this CLAUDE.md file
2. Review `FUTURE_IMPROVEMENTS_PLAN.md` for design decisions
3. Read existing test cases for examples
4. Examine similar features in the codebase

### For Bug Reports
1. Include repro steps and sample input
2. Provide build configuration (`cmake --system-information`)
3. Include output of `./motion_search --help` (or equivalent)
4. Attach logs with `stderr` output

### For Feature Requests
1. Check `FUTURE_IMPROVEMENTS_PLAN.md` - might already be planned
2. Describe use case and motivation
3. Suggest API or CLI interface if applicable

---

## Conclusion

This guide should provide all the context needed for effective development on motion-search. The codebase is well-structured, thoroughly tested, and ready for the improvements outlined in the future plan.

**Key Takeaways:**
- Modern C++17 with Highway SIMD - no legacy intrinsics
- Comprehensive test suite - maintain coverage
- Lightweight dependencies - justify new additions
- Clear roadmap - see FUTURE_IMPROVEMENTS_PLAN.md

Happy coding! 🚀

---

**Document Version:** 1.0
**Last Updated:** 2025-11-14
**Maintained by:** motion-search development team
**Questions?** Open an issue on GitHub
