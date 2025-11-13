/*
 * Tests for frame operations
 * Validates frame border extension functionality
 */

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "common.h"
#include "frame.h"
}

class FrameTest : public ::testing::Test {
protected:
  void fillPattern(uint8_t *data, int width, int height, int stride) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        data[y * stride + x] = static_cast<uint8_t>((x + y) % 256);
      }
    }
  }

  void fillConstant(uint8_t *data, int width, int height, int stride,
                    uint8_t value) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        data[y * stride + x] = value;
      }
    }
  }
};

TEST_F(FrameTest, ExtendFrame_BasicFunctionality) {
  const int width = 16;
  const int height = 16;
  const int pad_x = 8;
  const int pad_y = 8;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  // Fill the center region with a pattern
  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  fillPattern(center, width, height, stride);

  DIM dim = {width, height};
  extend_frame(frame.data() + pad_y * stride + pad_x, stride, dim, pad_x,
               pad_y);

  // Verify top border - should replicate first row
  // The top padding rows should be copies of the first row (including
  // left/right padding)
  for (int y = 0; y < pad_y; y++) {
    for (int x = 0; x < stride; x++) {
      EXPECT_EQ(frame[pad_y * stride + x], frame[y * stride + x])
          << "Top padding row " << y << " col " << x
          << " not replicated correctly";
    }
  }

  // Verify left border
  for (int y = pad_y; y < pad_y + height; y++) {
    EXPECT_EQ(center[(y - pad_y) * stride], frame[y * stride + 0])
        << "Left border at y=" << y << " not extended correctly";
  }
}

TEST_F(FrameTest, ExtendFrame_ConstantImage) {
  const int width = 32;
  const int height = 32;
  const int pad_x = 16;
  const int pad_y = 16;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  fillConstant(center, width, height, stride, 128);

  DIM dim = {width, height};
  extend_frame(center, stride, dim, pad_x, pad_y);

  // For a constant image, all extended pixels should equal the constant value
  for (int y = 0; y < total_height; y++) {
    for (int x = 0; x < stride; x++) {
      EXPECT_EQ(128, frame[y * stride + x])
          << "Extended pixel at (" << x << ", " << y
          << ") incorrect for constant image";
    }
  }
}

TEST_F(FrameTest, ExtendFrame_SmallPadding) {
  const int width = 16;
  const int height = 16;
  const int pad_x = 2;
  const int pad_y = 2;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      center[y * stride + x] = static_cast<uint8_t>(x + y);
    }
  }

  DIM dim = {width, height};
  extend_frame(center, stride, dim, pad_x, pad_y);

  // Verify corners are replicated
  uint8_t top_left = center[0];
  uint8_t top_right = center[width - 1];
  uint8_t bottom_left = center[(height - 1) * stride];
  uint8_t bottom_right = center[(height - 1) * stride + width - 1];

  EXPECT_EQ(top_left, frame[0]);
  EXPECT_EQ(top_right, frame[stride - 1]);
  EXPECT_EQ(bottom_left, frame[(total_height - 1) * stride]);
  EXPECT_EQ(bottom_right, frame[(total_height - 1) * stride + stride - 1]);
}

TEST_F(FrameTest, ExtendFrame_AsymmetricPadding) {
  const int width = 16;
  const int height = 16;
  const int pad_x = 4;
  const int pad_y = 8;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  fillPattern(center, width, height, stride);

  DIM dim = {width, height};
  extend_frame(center, stride, dim, pad_x, pad_y);

  // Verify that the center region is unchanged
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t expected = static_cast<uint8_t>((x + y) % 256);
      EXPECT_EQ(expected, center[y * stride + x])
          << "Center region modified at (" << x << ", " << y << ")";
    }
  }
}

TEST_F(FrameTest, ExtendFrame_StandardMotionSearchPadding) {
  // Test with typical motion search padding values
  const int width = 320;
  const int height = 180;
  const int pad_x = HORIZONTAL_PADDING;
  const int pad_y = VERTICAL_PADDING;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  fillPattern(center, width, height, stride);

  DIM dim = {width, height};
  extend_frame(center, stride, dim, pad_x, pad_y);

  // Verify edge replication at specific locations
  // Top-left region
  uint8_t expected_top_left = center[0];
  for (int y = 0; y < pad_y; y++) {
    for (int x = 0; x < pad_x; x++) {
      EXPECT_EQ(expected_top_left, frame[y * stride + x])
          << "Top-left padding incorrect at (" << x << ", " << y << ")";
    }
  }

  // Bottom-right region
  uint8_t expected_bottom_right = center[(height - 1) * stride + (width - 1)];
  for (int y = pad_y + height; y < total_height; y++) {
    for (int x = pad_x + width; x < stride; x++) {
      EXPECT_EQ(expected_bottom_right, frame[y * stride + x])
          << "Bottom-right padding incorrect at (" << x << ", " << y << ")";
    }
  }
}

TEST_F(FrameTest, ExtendFrame_MinimalSize) {
  // Test with minimal frame size
  const int width = 4;
  const int height = 4;
  const int pad_x = 2;
  const int pad_y = 2;
  const int stride = width + 2 * pad_x;
  const int total_height = height + 2 * pad_y;

  std::vector<uint8_t> frame(stride * total_height, 0);

  uint8_t *center = frame.data() + pad_y * stride + pad_x;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      center[y * stride + x] = static_cast<uint8_t>(y * width + x);
    }
  }

  DIM dim = {width, height};
  extend_frame(center, stride, dim, pad_x, pad_y);

  // Just verify no crashes and basic corner replication
  EXPECT_EQ(center[0], frame[0]);
  EXPECT_EQ(center[width - 1], frame[stride - 1]);
}
