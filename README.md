# motion-search

[![Tests](https://github.com/cosmin/motion-search/actions/workflows/tests.yml/badge.svg)](https://github.com/cosmin/motion-search/actions/workflows/tests.yml)
[![Format Check](https://github.com/cosmin/motion-search/actions/workflows/format-check.yml/badge.svg)](https://github.com/cosmin/motion-search/actions/workflows/format-check.yml)

Perform motion search and compute motion vectors and residual information in order to extract features for predicting video compressibility.

Contents:

- [Requirements](#requirements)
- [Building](#building)
- [Testing](#testing)
- [Running](#running)
  - [Options](#options)
- [License](#license)

## Requirements

- [CMake](https://cmake.org/) (>= 3.10)
- C++17 compatible compiler
- `clang-format`
- [Google Highway](https://github.com/google/highway) (automatically fetched by CMake)
- [Google Abseil](https://abseil.io/) (automatically fetched by CMake)
- **Optional:** [FFmpeg](https://ffmpeg.org/) libraries (libavformat, libavcodec, libavutil, libswscale) for MP4/MKV/WebM support

Under Linux, to install CMake and FFmpeg, install the `cmake`, `clang-format`, and `libavformat-dev` packages from your distribution's package manager:

```bash
# Ubuntu/Debian
sudo apt-get install cmake clang-format libavformat-dev libavcodec-dev libavutil-dev libswscale-dev

# Fedora/RHEL
sudo dnf install cmake clang-tools-extra ffmpeg-devel
```

Under macOS, you can install CMake and FFmpeg using [Homebrew](https://brew.sh/):

```bash
brew install cmake clang-format ffmpeg
```

## Building

Create a build folder and run CMake, then `make`:

``` shell
mkdir build
cd build
cmake ..
make -j8
```

The `motion-search` executable will be created in the `build/bin` folder.

**Note**: By default, CMake will automatically download and build [Google Highway](https://github.com/google/highway) v1.2.0 for portable SIMD operations. Highway provides cross-platform vectorization that works on x86 (SSE2/AVX2/AVX-512), ARM (NEON), and other architectures.

### Build Options

#### SIMD Support (USE_HIGHWAY_SIMD)

By default, the project builds with Highway SIMD support for optimal performance. You can disable Highway and use pure C implementations instead:

```shell
cmake .. -DUSE_HIGHWAY_SIMD=OFF
```

**When to disable Highway:**
- Systems where Highway is not compatible or available
- Research or debugging purposes where predictable C code is preferred
- Environments with limited build resources

**Performance impact:** Pure C implementations are significantly slower (2-4x) than SIMD-optimized code, but produce identical results for testing and validation.

#### FFmpeg Input Support (ENABLE_FFMPEG) - Phase 3

By default, the project builds **without** FFmpeg support and can only read Y4M and raw YUV files. To enable support for additional video formats (MP4, MKV, AVI, WebM, etc.), enable FFmpeg:

```shell
cmake .. -DENABLE_FFMPEG=ON
make -j8
```

**Requirements:**
- FFmpeg libraries must be installed on your system (see [Requirements](#requirements))
- pkg-config must be available to locate FFmpeg libraries

**Supported formats with FFmpeg:**
- Container formats: MP4, MKV, AVI, WebM, MOV, FLV, and more
- Video codecs: H.264, H.265/HEVC, VP9, AV1, MPEG-4, and more

**Usage with FFmpeg:**
```shell
# Explicitly use FFmpeg for any format
./bin/motion_search --input=video.mp4 --use_ffmpeg --output=results.csv

# Auto-detect: FFmpeg used automatically for non-YUV/Y4M files
./bin/motion_search --input=video.mkv --output=results.csv
```

**When to enable FFmpeg:**
- You need to analyze videos in compressed formats (MP4, MKV, etc.)
- You want to avoid manual conversion to Y4M/YUV
- You're building a pipeline that processes various input formats

**Note:** Y4M and raw YUV files are always handled by native readers (faster and simpler), regardless of FFmpeg availability.

## Testing

This project includes a comprehensive test suite built with [Google Test](https://github.com/google/googletest).

### Building Tests

Tests are built automatically when you build the project. If you want to disable tests, you can set `BUILD_TESTING=OFF`:

```shell
cmake .. -DBUILD_TESTING=OFF
```

### Generating Test Videos

The integration tests require test video files. Generate them using ffmpeg:

```shell
cd tests
./generate_test_videos.sh
```

This will create 13 test videos (320x180, 10 frames) in `tests/test_data/`:
- `testsrc.yuv` / `testsrc.y4m` - Moving test patterns
- `black.yuv`, `white.yuv`, `gray.yuv` - Solid colors
- `hgradient.yuv`, `vgradient.yuv` - Directional gradients
- `checkerboard.yuv` - High spatial frequency pattern
- `smptebars.yuv` - SMPTE color bars
- `moving_box.yuv` - Known motion vectors
- `mandelbrot.yuv` - Complex fractal pattern
- `single_frame.yuv`, `two_identical.yuv` - Edge cases

**Note**: Requires `ffmpeg` to be installed on your system.

### Running Tests

To run all tests:

```shell
cd build
ctest --output-on-failure
```

Or run individual test suites:

```shell
# SIMD primitive tests (SAD, MSE, variance, bidirectional MSE)
./bin/test_moments

# Frame operation tests (border extension)
./bin/test_frame

# Motion search algorithm tests
./bin/test_motion_search

# Integration tests (end-to-end video analysis)
./bin/test_integration
```

### Test Coverage

The test suite includes:

1. **test_moments** (21 tests) - SIMD primitive validation
   - Tests SAD, MSE, variance, and bidirectional MSE functions
   - Validates Highway SIMD optimizations match C reference implementations
   - Includes stress tests with 100 iterations

2. **test_frame** (6 tests) - Frame operation validation
   - Tests frame border extension for motion search padding
   - Validates edge replication behavior

3. **test_motion_search** (7 tests) - Algorithm validation
   - Tests spatial search, motion search, and bidirectional motion search
   - Tests various frame patterns and motion scenarios

4. **test_integration** (8 tests) - End-to-end validation
   - Tests ComplexityAnalyzer with YUV and Y4M video readers
   - Tests GOP structure and frame type handling
   - Validates output consistency

## Running

### Modern Command-Line Syntax (Recommended)

The modern syntax uses explicit flags with `--flag=value`:

```shell
# Y4M file (no dimensions needed)
./bin/motion_search --input=video.y4m --output=results.csv

# Raw YUV file (dimensions required)
./bin/motion_search --input=input.yuv --width=1920 --height=1080 --output=stats.csv

# MP4/MKV file (requires FFmpeg, dimensions auto-detected)
./bin/motion_search --input=video.mp4 --use_ffmpeg --output=results.csv

# With GOP and B-frame settings
./bin/motion_search --input=video.y4m --gop_size=60 --bframes=2 --output=results.csv
```

### Legacy Syntax (Backward Compatible)

The legacy syntax is still supported:

```shell
motion_search input.yuv -W=1920 -H=1080 stats.txt -g=150 -b=2
```

### Input Formats

**Without FFmpeg (default build):**
- `.y4m` - Y4M files (dimensions auto-detected)
- `.yuv` - Raw YUV420p files (requires `--width` and `--height`)

**With FFmpeg (when built with `-DENABLE_FFMPEG=ON`):**
- `.mp4`, `.mkv`, `.avi`, `.webm`, `.mov` - Any FFmpeg-supported format
- Use `--use_ffmpeg` flag or rely on auto-detection for non-YUV/Y4M files

### Output Format

The output file will be a CSV file containing the following columns:

- `picNum`: picture number
- `picType`: picture type (I, P, B)
- `count_I`: number of I blocks
- `count_P`: number of P blocks
- `count_B`: number of B blocks
- `error`: mean squared error
- `bits`: number of bits estimated

The tool will print extra information to stderr, such as the number of frames processed, per-GOP stats, and total algorithm execution time.

### Options

**Input options:**
- `--input=<file>` - Input video file (required)
- `--width=<n>` - Video width in pixels (required for raw YUV)
- `--height=<n>` - Video height in pixels (required for raw YUV)
- `--use_ffmpeg` - Use FFmpeg for input decoding (auto-detected for non-YUV/Y4M)

**Analysis options:**
- `--gop_size=<n>` - GOP size for simulation (default: 150)
- `--bframes=<n>` - Number of consecutive B-frames (default: 0)
- `--frames=<n>` - Number of frames to process (0 = all, default: 0)

**Output options:**
- `--output=<file>` - Output CSV file (use '-' for stdout, required)
- `--format=<fmt>` - Output format: csv (default), json, xml (Phase 2)

**Legacy flags (backward compatibility):**
- `-W=<n>` - Same as `--width`
- `-H=<n>` - Same as `--height`
- `-g=<n>` - Same as `--gop_size`
- `-b=<n>` - Same as `--bframes`
- `-n=<n>` - Same as `--frames`

See `motion_search --help` for complete usage information.

Note that currently when using B-frames:
* code may read more than requested number of frames to complete last subgop
* if there aren't enough frames to complete last subgop then trailing
  frames after the previous complete subgop will be ignored

## License

This source code is licensed under the BSD3 license found in the
LICENSE file in the root directory of this source tree.
