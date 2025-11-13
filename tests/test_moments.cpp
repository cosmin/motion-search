/*
 * Tests for SIMD primitive functions (moments)
 * Validates that SSE2 optimized versions produce identical results to reference
 * C implementations
 */

#include <cstring>
#include <gtest/gtest.h>
#include <random>
#include <vector>

extern "C" {
#include "common.h"
#include "moments.h"
}

class MomentsTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize random number generator with fixed seed for reproducibility
    rng.seed(12345);
    dist = std::uniform_int_distribution<int>(0, 255);
  }

  // Generate random 8-bit data
  void fillRandom(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
      data[i] = static_cast<uint8_t>(dist(rng));
    }
  }

  // Generate specific patterns
  void fillGradient(uint8_t *data, int width, int height, int stride) {
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

  void fillCheckerboard(uint8_t *data, int width, int height, int stride,
                        int blockSize = 8) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        data[y * stride + x] =
            ((x / blockSize) + (y / blockSize)) % 2 ? 255 : 0;
      }
    }
  }

  std::mt19937 rng;
  std::uniform_int_distribution<int> dist;
};

// ============================================================================
// SAD (Sum of Absolute Differences) Tests
// ============================================================================

TEST_F(MomentsTest, SAD16_RandomData) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int sad_c = fastSAD16_c(current.data(), reference.data(), stride, block_width,
                          block_height, INT32_MAX);
  int sad_opt = fastSAD16(current.data(), reference.data(), stride, block_width,
                          block_height, INT32_MAX);

  EXPECT_EQ(sad_c, sad_opt) << "SAD16 optimized version differs from reference";
}

TEST_F(MomentsTest, SAD8_RandomData) {
  const int stride = 64;
  const int block_width = 8;
  const int block_height = 8;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int sad_c = fastSAD8_c(current.data(), reference.data(), stride, block_width,
                         block_height, INT32_MAX);
  int sad_opt = fastSAD8(current.data(), reference.data(), stride, block_width,
                         block_height, INT32_MAX);

  EXPECT_EQ(sad_c, sad_opt) << "SAD8 optimized version differs from reference";
}

TEST_F(MomentsTest, SAD4_RandomData) {
  const int stride = 64;
  const int block_width = 4;
  const int block_height = 4;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int sad_c = fastSAD4_c(current.data(), reference.data(), stride, block_width,
                         block_height, INT32_MAX);
  int sad_opt = fastSAD4(current.data(), reference.data(), stride, block_width,
                         block_height, INT32_MAX);

  EXPECT_EQ(sad_c, sad_opt) << "SAD4 optimized version differs from reference";
}

TEST_F(MomentsTest, SAD16_IdenticalBlocks) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> data(stride * block_height);

  fillRandom(data.data(), data.size());

  int sad_c = fastSAD16_c(data.data(), data.data(), stride, block_width,
                          block_height, INT32_MAX);
  int sad_opt = fastSAD16(data.data(), data.data(), stride, block_width,
                          block_height, INT32_MAX);

  EXPECT_EQ(0, sad_c) << "SAD of identical blocks should be 0";
  EXPECT_EQ(sad_c, sad_opt);
}

TEST_F(MomentsTest, SAD16_MaxDifference) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> black(stride * block_height, 0);
  std::vector<uint8_t> white(stride * block_height, 255);

  int sad_c = fastSAD16_c(black.data(), white.data(), stride, block_width,
                          block_height, INT32_MAX);
  int sad_opt = fastSAD16(black.data(), white.data(), stride, block_width,
                          block_height, INT32_MAX);

  int expected = 255 * block_width * block_height;
  EXPECT_EQ(expected, sad_c) << "SAD of black vs white should be max";
  EXPECT_EQ(sad_c, sad_opt);
}

TEST_F(MomentsTest, SAD_EarlyTermination) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> black(stride * block_height, 0);
  std::vector<uint8_t> white(stride * block_height, 255);

  int min_SAD = 1000; // Early termination threshold

  int sad_c = fastSAD16_c(black.data(), white.data(), stride, block_width,
                          block_height, min_SAD);
  int sad_opt = fastSAD16(black.data(), white.data(), stride, block_width,
                          block_height, min_SAD);

  // Should terminate early and both should return value >= min_SAD
  // Note: C and SSE2 implementations may return different values when
  // early-terminating, as long as both are >= min_SAD
  EXPECT_GE(sad_c, min_SAD);
  EXPECT_GE(sad_opt, min_SAD);
}

// ============================================================================
// Variance Tests
// ============================================================================

TEST_F(MomentsTest, Variance16_RandomData) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> data(stride * block_height);

  fillRandom(data.data(), data.size());

  int var_c = fast_variance16_c(data.data(), stride, block_width, block_height);
  int var_opt = fast_variance16(data.data(), stride, block_width, block_height);

  EXPECT_EQ(var_c, var_opt)
      << "Variance16 optimized version differs from reference";
}

TEST_F(MomentsTest, Variance8_RandomData) {
  const int stride = 64;
  const int block_width = 8;
  const int block_height = 8;
  std::vector<uint8_t> data(stride * block_height);

  fillRandom(data.data(), data.size());

  int var_c = fast_variance8_c(data.data(), stride, block_width, block_height);
  int var_opt = fast_variance8(data.data(), stride, block_width, block_height);

  EXPECT_EQ(var_c, var_opt)
      << "Variance8 optimized version differs from reference";
}

TEST_F(MomentsTest, Variance4_RandomData) {
  const int stride = 64;
  const int block_width = 4;
  const int block_height = 4;
  std::vector<uint8_t> data(stride * block_height);

  fillRandom(data.data(), data.size());

  int var_c = fast_variance4_c(data.data(), stride, block_width, block_height);
  int var_opt = fast_variance4(data.data(), stride, block_width, block_height);

  EXPECT_EQ(var_c, var_opt)
      << "Variance4 optimized version differs from reference";
}

TEST_F(MomentsTest, Variance16_ConstantBlock) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> data(stride * block_height);

  fillConstant(data.data(), block_width, block_height, stride, 128);

  int var_c = fast_variance16_c(data.data(), stride, block_width, block_height);
  int var_opt = fast_variance16(data.data(), stride, block_width, block_height);

  EXPECT_EQ(0, var_c) << "Variance of constant block should be 0";
  EXPECT_EQ(var_c, var_opt);
}

TEST_F(MomentsTest, Variance16_Gradient) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> data(stride * block_height);

  fillGradient(data.data(), block_width, block_height, stride);

  int var_c = fast_variance16_c(data.data(), stride, block_width, block_height);
  int var_opt = fast_variance16(data.data(), stride, block_width, block_height);

  EXPECT_GT(var_c, 0) << "Variance of gradient should be > 0";
  EXPECT_EQ(var_c, var_opt);
}

// ============================================================================
// MSE (Mean Squared Error) Tests
// ============================================================================

TEST_F(MomentsTest, MSE16_RandomData) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int mse_c = fast_calc_mse16_c(current.data(), reference.data(), stride,
                                block_width, block_height);
  int mse_opt = fast_calc_mse16(current.data(), reference.data(), stride,
                                block_width, block_height);

  EXPECT_EQ(mse_c, mse_opt) << "MSE16 optimized version differs from reference";
}

TEST_F(MomentsTest, MSE8_RandomData) {
  const int stride = 64;
  const int block_width = 8;
  const int block_height = 8;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int mse_c = fast_calc_mse8_c(current.data(), reference.data(), stride,
                               block_width, block_height);
  int mse_opt = fast_calc_mse8(current.data(), reference.data(), stride,
                               block_width, block_height);

  EXPECT_EQ(mse_c, mse_opt) << "MSE8 optimized version differs from reference";
}

TEST_F(MomentsTest, MSE4_RandomData) {
  const int stride = 64;
  const int block_width = 4;
  const int block_height = 4;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> reference(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(reference.data(), reference.size());

  int mse_c = fast_calc_mse4_c(current.data(), reference.data(), stride,
                               block_width, block_height);
  int mse_opt = fast_calc_mse4(current.data(), reference.data(), stride,
                               block_width, block_height);

  EXPECT_EQ(mse_c, mse_opt) << "MSE4 optimized version differs from reference";
}

TEST_F(MomentsTest, MSE16_IdenticalBlocks) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> data(stride * block_height);

  fillRandom(data.data(), data.size());

  int mse_c = fast_calc_mse16_c(data.data(), data.data(), stride, block_width,
                                block_height);
  int mse_opt = fast_calc_mse16(data.data(), data.data(), stride, block_width,
                                block_height);

  EXPECT_EQ(0, mse_c) << "MSE of identical blocks should be 0";
  EXPECT_EQ(mse_c, mse_opt);
}

// ============================================================================
// Bidirectional MSE Tests
// ============================================================================

TEST_F(MomentsTest, BidirMSE16_RandomData) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> ref1(stride * block_height);
  std::vector<uint8_t> ref2(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(ref1.data(), ref1.size());
  fillRandom(ref2.data(), ref2.size());

  MV td = {0, 0};

  int mse_c = fast_bidir_mse16_c(current.data(), ref1.data(), ref2.data(),
                                 stride, block_width, block_height, &td);
  int mse_opt = fast_bidir_mse16(current.data(), ref1.data(), ref2.data(),
                                 stride, block_width, block_height, &td);

  EXPECT_EQ(mse_c, mse_opt)
      << "BidirMSE16 optimized version differs from reference";
}

TEST_F(MomentsTest, BidirMSE8_RandomData) {
  const int stride = 64;
  const int block_width = 8;
  const int block_height = 8;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> ref1(stride * block_height);
  std::vector<uint8_t> ref2(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(ref1.data(), ref1.size());
  fillRandom(ref2.data(), ref2.size());

  MV td = {0, 0};

  int mse_c = fast_bidir_mse8_c(current.data(), ref1.data(), ref2.data(),
                                stride, block_width, block_height, &td);
  int mse_opt = fast_bidir_mse8(current.data(), ref1.data(), ref2.data(),
                                stride, block_width, block_height, &td);

  EXPECT_EQ(mse_c, mse_opt)
      << "BidirMSE8 optimized version differs from reference";
}

TEST_F(MomentsTest, BidirMSE4_RandomData) {
  const int stride = 64;
  const int block_width = 4;
  const int block_height = 4;
  std::vector<uint8_t> current(stride * block_height);
  std::vector<uint8_t> ref1(stride * block_height);
  std::vector<uint8_t> ref2(stride * block_height);

  fillRandom(current.data(), current.size());
  fillRandom(ref1.data(), ref1.size());
  fillRandom(ref2.data(), ref2.size());

  MV td = {0, 0};

  int mse_c = fast_bidir_mse4_c(current.data(), ref1.data(), ref2.data(),
                                stride, block_width, block_height, &td);
  int mse_opt = fast_bidir_mse4(current.data(), ref1.data(), ref2.data(),
                                stride, block_width, block_height, &td);

  EXPECT_EQ(mse_c, mse_opt)
      << "BidirMSE4 optimized version differs from reference";
}

// ============================================================================
// Stress Tests with Multiple Iterations
// ============================================================================

TEST_F(MomentsTest, SAD16_StressTest) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;

  for (int iter = 0; iter < 100; iter++) {
    std::vector<uint8_t> current(stride * block_height);
    std::vector<uint8_t> reference(stride * block_height);

    fillRandom(current.data(), current.size());
    fillRandom(reference.data(), reference.size());

    int sad_c = fastSAD16_c(current.data(), reference.data(), stride,
                            block_width, block_height, INT32_MAX);
    int sad_opt = fastSAD16(current.data(), reference.data(), stride,
                            block_width, block_height, INT32_MAX);

    EXPECT_EQ(sad_c, sad_opt) << "Iteration " << iter << " failed";
  }
}

TEST_F(MomentsTest, Variance16_StressTest) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;

  for (int iter = 0; iter < 100; iter++) {
    std::vector<uint8_t> data(stride * block_height);
    fillRandom(data.data(), data.size());

    int var_c =
        fast_variance16_c(data.data(), stride, block_width, block_height);
    int var_opt =
        fast_variance16(data.data(), stride, block_width, block_height);

    EXPECT_EQ(var_c, var_opt) << "Iteration " << iter << " failed";
  }
}

TEST_F(MomentsTest, MSE16_StressTest) {
  const int stride = 64;
  const int block_width = 16;
  const int block_height = 16;

  for (int iter = 0; iter < 100; iter++) {
    std::vector<uint8_t> current(stride * block_height);
    std::vector<uint8_t> reference(stride * block_height);

    fillRandom(current.data(), current.size());
    fillRandom(reference.data(), reference.size());

    int mse_c = fast_calc_mse16_c(current.data(), reference.data(), stride,
                                  block_width, block_height);
    int mse_opt = fast_calc_mse16(current.data(), reference.data(), stride,
                                  block_width, block_height);

    EXPECT_EQ(mse_c, mse_opt) << "Iteration " << iter << " failed";
  }
}
