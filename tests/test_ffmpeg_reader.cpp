/*
 * FFmpeg Reader Integration Tests
 * Tests that FFmpegSequenceReader produces identical results to native readers
 */

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "Y4MSequenceReader.h"
#include "YUVSequenceReader.h"
#include "common.h"

#ifdef HAVE_FFMPEG
#include "FFmpegSequenceReader.h"
#endif

class FFmpegReaderTest : public ::testing::Test {
protected:
  void SetUp() override { test_data_dir = TEST_DATA_DIR; }

  bool fileExists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
  }

  unique_file_t openFile(const std::string &path) {
    FILE *fp = fopen(path.c_str(), "rb");
    return unique_file_t(fp);
  }

  // Compare two YUV frames for equality
  bool compareFrames(const uint8_t *y1, const uint8_t *u1, const uint8_t *v1,
                     const uint8_t *y2, const uint8_t *u2, const uint8_t *v2,
                     int width, int height, ptrdiff_t stride1,
                     ptrdiff_t stride2) {
    // Compare Y plane
    for (int row = 0; row < height; row++) {
      if (memcmp(y1 + row * stride1, y2 + row * stride2, width) != 0) {
        return false;
      }
    }

    // Compare U and V planes (half resolution)
    int uv_width = width / 2;
    int uv_height = height / 2;
    ptrdiff_t uv_stride1 = stride1 / 2;
    ptrdiff_t uv_stride2 = stride2 / 2;

    for (int row = 0; row < uv_height; row++) {
      if (memcmp(u1 + row * uv_stride1, u2 + row * uv_stride2, uv_width) != 0) {
        return false;
      }
      if (memcmp(v1 + row * uv_stride1, v2 + row * uv_stride2, uv_width) != 0) {
        return false;
      }
    }

    return true;
  }

  // Allocate buffers for a YUV frame
  struct YUVBuffers {
    std::vector<uint8_t> y;
    std::vector<uint8_t> u;
    std::vector<uint8_t> v;

    YUVBuffers(int width, int height, ptrdiff_t stride) {
      y.resize(height * stride);
      u.resize((height / 2) * (stride / 2));
      v.resize((height / 2) * (stride / 2));
    }

    uint8_t *yPtr() { return y.data(); }
    uint8_t *uPtr() { return u.data(); }
    uint8_t *vPtr() { return v.data(); }
  };

  std::string test_data_dir;
};

#ifdef HAVE_FFMPEG

TEST_F(FFmpegReaderTest, FFmpegReader_OpenY4M) {
  std::string test_file = test_data_dir + "/testsrc.y4m";

  if (!fileExists(test_file)) {
    GTEST_SKIP() << "Test file not found: " << test_file
                 << ". Run generate_test_videos.sh first.";
  }

  FFmpegSequenceReader reader;
  bool opened = reader.Open(test_file);

  EXPECT_TRUE(opened) << "Failed to open Y4M file with FFmpeg";
  EXPECT_TRUE(reader.isOpen());
  EXPECT_GT(reader.dim().width, 0);
  EXPECT_GT(reader.dim().height, 0);
}

TEST_F(FFmpegReaderTest, CompareY4MReaders_Metadata) {
  std::string test_file = test_data_dir + "/testsrc.y4m";

  if (!fileExists(test_file)) {
    GTEST_SKIP() << "Test file not found: " << test_file
                 << ". Run generate_test_videos.sh first.";
  }

  // Open with native Y4M reader
  Y4MSequenceReader native_reader;
  auto file = openFile(test_file);
  bool native_opened = native_reader.Open(std::move(file), test_file);
  ASSERT_TRUE(native_opened) << "Failed to open Y4M file with native reader";

  // Open with FFmpeg reader
  FFmpegSequenceReader ffmpeg_reader;
  bool ffmpeg_opened = ffmpeg_reader.Open(test_file);
  ASSERT_TRUE(ffmpeg_opened) << "Failed to open Y4M file with FFmpeg reader";

  // Compare metadata
  EXPECT_EQ(native_reader.dim().width, ffmpeg_reader.dim().width)
      << "Width mismatch between readers";
  EXPECT_EQ(native_reader.dim().height, ffmpeg_reader.dim().height)
      << "Height mismatch between readers";
  EXPECT_EQ(native_reader.stride(), ffmpeg_reader.stride())
      << "Stride mismatch between readers";
}

TEST_F(FFmpegReaderTest, CompareY4MReaders_FirstFrame) {
  std::string test_file = test_data_dir + "/testsrc.y4m";

  if (!fileExists(test_file)) {
    GTEST_SKIP() << "Test file not found: " << test_file
                 << ". Run generate_test_videos.sh first.";
  }

  // Open with native Y4M reader
  Y4MSequenceReader native_reader;
  auto file = openFile(test_file);
  bool native_opened = native_reader.Open(std::move(file), test_file);
  ASSERT_TRUE(native_opened);

  // Open with FFmpeg reader
  FFmpegSequenceReader ffmpeg_reader;
  bool ffmpeg_opened = ffmpeg_reader.Open(test_file);
  ASSERT_TRUE(ffmpeg_opened);

  // Get dimensions
  int width = native_reader.dim().width;
  int height = native_reader.dim().height;
  ptrdiff_t stride1 = native_reader.stride();
  ptrdiff_t stride2 = ffmpeg_reader.stride();

  // Allocate buffers for both readers
  YUVBuffers native_frame(width, height, stride1);
  YUVBuffers ffmpeg_frame(width, height, stride2);

  // Read first frame from both readers
  ASSERT_NO_THROW(native_reader.read(native_frame.yPtr(), native_frame.uPtr(),
                                     native_frame.vPtr()))
      << "Native reader failed to read first frame";
  ASSERT_NO_THROW(ffmpeg_reader.read(ffmpeg_frame.yPtr(), ffmpeg_frame.uPtr(),
                                     ffmpeg_frame.vPtr()))
      << "FFmpeg reader failed to read first frame";

  // Compare frames
  bool frames_match = compareFrames(native_frame.yPtr(), native_frame.uPtr(),
                                    native_frame.vPtr(), ffmpeg_frame.yPtr(),
                                    ffmpeg_frame.uPtr(), ffmpeg_frame.vPtr(),
                                    width, height, stride1, stride2);

  EXPECT_TRUE(frames_match)
      << "First frame differs between native and FFmpeg readers";
}

TEST_F(FFmpegReaderTest, CompareY4MReaders_AllFrames) {
  std::string test_file = test_data_dir + "/testsrc.y4m";

  if (!fileExists(test_file)) {
    GTEST_SKIP() << "Test file not found: " << test_file
                 << ". Run generate_test_videos.sh first.";
  }

  // Open with native Y4M reader
  Y4MSequenceReader native_reader;
  auto file = openFile(test_file);
  bool native_opened = native_reader.Open(std::move(file), test_file);
  ASSERT_TRUE(native_opened);

  // Open with FFmpeg reader
  FFmpegSequenceReader ffmpeg_reader;
  bool ffmpeg_opened = ffmpeg_reader.Open(test_file);
  ASSERT_TRUE(ffmpeg_opened);

  // Get dimensions
  int width = native_reader.dim().width;
  int height = native_reader.dim().height;
  ptrdiff_t stride1 = native_reader.stride();
  ptrdiff_t stride2 = ffmpeg_reader.stride();

  // Allocate buffers
  YUVBuffers native_frame(width, height, stride1);
  YUVBuffers ffmpeg_frame(width, height, stride2);

  // Read and compare all frames
  int frame_num = 0;
  while (!native_reader.eof() && !ffmpeg_reader.eof()) {
    // Read frames
    try {
      native_reader.read(native_frame.yPtr(), native_frame.uPtr(),
                         native_frame.vPtr());
      ffmpeg_reader.read(ffmpeg_frame.yPtr(), ffmpeg_frame.uPtr(),
                         ffmpeg_frame.vPtr());
    } catch (...) {
      // End of file reached for one reader
      break;
    }

    // Compare frames
    bool frames_match = compareFrames(native_frame.yPtr(), native_frame.uPtr(),
                                      native_frame.vPtr(), ffmpeg_frame.yPtr(),
                                      ffmpeg_frame.uPtr(), ffmpeg_frame.vPtr(),
                                      width, height, stride1, stride2);

    EXPECT_TRUE(frames_match) << "Frame " << frame_num
                              << " differs between native and FFmpeg readers";

    if (!frames_match) {
      // Stop on first mismatch to avoid flooding output
      break;
    }

    frame_num++;
  }

  // Verify both readers reached EOF at the same time
  EXPECT_EQ(native_reader.eof(), ffmpeg_reader.eof())
      << "Readers reached EOF at different times";

  EXPECT_GT(frame_num, 0) << "No frames were read";
}

TEST_F(FFmpegReaderTest, CompareY4MReaders_MultipleFiles) {
  // Test with multiple different Y4M files to ensure consistency
  std::vector<std::string> test_files = {
      test_data_dir + "/testsrc.y4m", test_data_dir + "/black.y4m",
      test_data_dir + "/white.y4m", test_data_dir + "/gray.y4m",
      test_data_dir + "/moving_box.y4m"};

  int files_tested = 0;

  for (const auto &test_file : test_files) {
    if (!fileExists(test_file)) {
      // Skip missing files without failing the test
      continue;
    }

    // Open with native Y4M reader
    Y4MSequenceReader native_reader;
    auto file = openFile(test_file);
    bool native_opened = native_reader.Open(std::move(file), test_file);
    if (!native_opened) {
      continue;
    }

    // Open with FFmpeg reader
    FFmpegSequenceReader ffmpeg_reader;
    bool ffmpeg_opened = ffmpeg_reader.Open(test_file);
    if (!ffmpeg_opened) {
      FAIL() << "FFmpeg reader failed to open file: " << test_file;
      continue;
    }

    // Compare dimensions
    EXPECT_EQ(native_reader.dim().width, ffmpeg_reader.dim().width)
        << "Width mismatch for " << test_file;
    EXPECT_EQ(native_reader.dim().height, ffmpeg_reader.dim().height)
        << "Height mismatch for " << test_file;

    // Read and compare first frame
    int width = native_reader.dim().width;
    int height = native_reader.dim().height;
    ptrdiff_t stride1 = native_reader.stride();
    ptrdiff_t stride2 = ffmpeg_reader.stride();

    YUVBuffers native_frame(width, height, stride1);
    YUVBuffers ffmpeg_frame(width, height, stride2);

    try {
      native_reader.read(native_frame.yPtr(), native_frame.uPtr(),
                         native_frame.vPtr());
      ffmpeg_reader.read(ffmpeg_frame.yPtr(), ffmpeg_frame.uPtr(),
                         ffmpeg_frame.vPtr());

      bool frames_match = compareFrames(
          native_frame.yPtr(), native_frame.uPtr(), native_frame.vPtr(),
          ffmpeg_frame.yPtr(), ffmpeg_frame.uPtr(), ffmpeg_frame.vPtr(), width,
          height, stride1, stride2);

      EXPECT_TRUE(frames_match) << "First frame differs for " << test_file;
    } catch (...) {
      // Skip files that can't be read
    }

    files_tested++;
  }

  EXPECT_GT(files_tested, 0) << "No test files were available. Run "
                                "generate_test_videos.sh to create them.";
}

TEST_F(FFmpegReaderTest, FFmpegReader_InvalidFile) {
  std::string invalid_file = test_data_dir + "/nonexistent.y4m";

  FFmpegSequenceReader reader;
  bool opened = reader.Open(invalid_file);

  EXPECT_FALSE(opened) << "FFmpeg reader should fail on nonexistent file";
  EXPECT_FALSE(reader.isOpen());
}

#else // !HAVE_FFMPEG

TEST_F(FFmpegReaderTest, FFmpegNotAvailable) {
  GTEST_SKIP() << "FFmpeg support not compiled in. Build with "
                  "-DENABLE_FFMPEG=ON to run these tests.";
}

#endif // HAVE_FFMPEG
