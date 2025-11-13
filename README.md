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

- [CMake](https://cmake.org/) (>= 3.1)
- `clang-format`

Under Linux, to install CMake, install the `cmake` and `clang-format` packages from your distribution's package manager.

Under macOS, you can install CMake using [Homebrew](https://brew.sh/):

```bash
brew install cmake clang-format
```

## Building

Create a build folder and run CMake, then `make`:

``` shell
mkdir build
cd build
cmake ..
make -j $(nproc)
```

The `motion-search` executable will be created in the `build/bin` folder.

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
   - Validates SSE2 optimizations match C reference implementations
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

The basic usage is:

``` shell
motion-search <input> -W=<width> -H=<height> <output>
```

For example:

``` shell
motion-search input.yuv -W=1920 -H=1080 stats.txt
```

The input file must be a `.yuv` or `.y4m` file.

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

You can use `-g` to control GOP size (default 150), `-n` to control
number of frames to read (default is to read all) and `-b` to control
number of consecutive B-frames in subgop (default 0).

See `motion-search -h` for more information.

Note that currently when using B-frames:
* code may read more than requested number of frames to complete last subgop
* if there aren't enough frames to complete last subgop then trailing
  frames after the previous complete subgop will be ignored

## License

This source code is licensed under the BSD3 license found in the
LICENSE file in the root directory of this source tree.
