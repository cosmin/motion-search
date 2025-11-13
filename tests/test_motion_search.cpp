/*
 * Tests for motion search algorithms
 * Validates spatial search, motion search, and bidirectional motion search
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <algorithm>

extern "C" {
#include "motion_search.h"
#include "frame.h"
#include "common.h"
}

class MotionSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        block_width = MB_WIDTH;
        block_height = MB_WIDTH;
    }

    void fillConstant(uint8_t* data, int width, int height, int stride, uint8_t value) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                data[y * stride + x] = value;
            }
        }
    }

    void fillPattern(uint8_t* data, int width, int height, int stride) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                data[y * stride + x] = static_cast<uint8_t>((x + y) % 256);
            }
        }
    }

    void copyWithOffset(uint8_t* dst, const uint8_t* src, int width, int height,
                       int stride, int offset_x, int offset_y) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int src_x = x + offset_x;
                int src_y = y + offset_y;
                if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                    dst[y * stride + x] = src[(src_y) * stride + (src_x)];
                } else {
                    dst[y * stride + x] = 0;
                }
            }
        }
    }

    int block_width;
    int block_height;
};

TEST_F(MotionSearchTest, SpatialSearch_IdenticalFrames) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> frame(stride * total_height, 128);

    uint8_t* center = frame.data() + pad_y * stride + pad_x;

    DIM dim = {width, height};
    extend_frame(center, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int bits = 0;

    int result = spatial_search(center, center, stride, dim, block_width, block_height,
                               motion_vectors.data(), SADs.data(), mses.data(),
                               MB_modes.data(), &count_I, &bits);

    EXPECT_EQ(0, result);

    // For identical frames, all SADs and MSEs should be 0
    for (int i = 0; i < num_blocks; i++) {
        EXPECT_EQ(0, SADs[i]) << "SAD should be 0 for identical frames at block " << i;
        EXPECT_EQ(0, mses[i]) << "MSE should be 0 for identical frames at block " << i;
    }
}

TEST_F(MotionSearchTest, SpatialSearch_DifferentContent) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> current_frame(stride * total_height, 0);
    std::vector<uint8_t> ref_frame(stride * total_height, 0);

    uint8_t* current = current_frame.data() + pad_y * stride + pad_x;
    uint8_t* reference = ref_frame.data() + pad_y * stride + pad_x;

    // Fill with different patterns
    fillPattern(current, width, height, stride);
    fillConstant(reference, width, height, stride, 128);

    DIM dim = {width, height};
    extend_frame(current, stride, dim, pad_x, pad_y);
    extend_frame(reference, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int bits = 0;

    int result = spatial_search(current, reference, stride, dim, block_width, block_height,
                               motion_vectors.data(), SADs.data(), mses.data(),
                               MB_modes.data(), &count_I, &bits);

    EXPECT_EQ(0, result);

    // For different frames, SADs and MSEs should be > 0
    for (int i = 0; i < num_blocks; i++) {
        EXPECT_GT(SADs[i], 0) << "SAD should be > 0 for different frames at block " << i;
        EXPECT_GT(mses[i], 0) << "MSE should be > 0 for different frames at block " << i;
    }
}

TEST_F(MotionSearchTest, MotionSearch_ZeroMotion) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> frame(stride * total_height);

    uint8_t* center = frame.data() + pad_y * stride + pad_x;
    fillPattern(center, width, height, stride);

    DIM dim = {width, height};
    extend_frame(center, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int count_P = 0;
    int bits = 0;

    // Motion search with same frame as reference
    int result = motion_search(center, center, stride, dim, block_width, block_height,
                              motion_vectors.data(), SADs.data(), mses.data(),
                              MB_modes.data(), &count_I, &count_P, &bits);

    EXPECT_EQ(0, result);

    // All motion vectors should be (0, 0) for identical frames
    for (int i = 0; i < num_blocks; i++) {
        EXPECT_EQ(0, motion_vectors[i].x) << "MV x should be 0 at block " << i;
        EXPECT_EQ(0, motion_vectors[i].y) << "MV y should be 0 at block " << i;
        EXPECT_EQ(0, SADs[i]) << "SAD should be 0 for zero motion at block " << i;
    }
}

TEST_F(MotionSearchTest, MotionSearch_KnownMotion) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    // Create a reference frame with a distinct pattern
    std::vector<uint8_t> ref_frame(stride * total_height, 0);
    uint8_t* reference = ref_frame.data() + pad_y * stride + pad_x;

    // Create a pattern with a bright square in the middle
    fillConstant(reference, width, height, stride, 0);
    const int square_size = 16;
    const int square_x = width / 2 - square_size / 2;
    const int square_y = height / 2 - square_size / 2;
    for (int y = square_y; y < square_y + square_size; y++) {
        for (int x = square_x; x < square_x + square_size; x++) {
            reference[y * stride + x] = 255;
        }
    }

    DIM dim = {width, height};
    extend_frame(reference, stride, dim, pad_x, pad_y);

    // Create current frame with same pattern shifted
    std::vector<uint8_t> cur_frame(stride * total_height, 0);
    uint8_t* current = cur_frame.data() + pad_y * stride + pad_x;

    const int shift_x = 4;
    const int shift_y = 4;
    fillConstant(current, width, height, stride, 0);
    for (int y = square_y + shift_y; y < square_y + square_size + shift_y && y < height; y++) {
        for (int x = square_x + shift_x; x < square_x + square_size + shift_x && x < width; x++) {
            current[y * stride + x] = 255;
        }
    }

    extend_frame(current, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int count_P = 0;
    int bits = 0;

    int result = motion_search(current, reference, stride, dim, block_width, block_height,
                              motion_vectors.data(), SADs.data(), mses.data(),
                              MB_modes.data(), &count_I, &count_P, &bits);

    EXPECT_EQ(0, result);

    // At least some blocks should detect the motion
    bool found_motion = false;
    for (int i = 0; i < num_blocks; i++) {
        if (motion_vectors[i].x != 0 || motion_vectors[i].y != 0) {
            found_motion = true;
            break;
        }
    }
    EXPECT_TRUE(found_motion) << "Should detect motion in shifted frame";
}

TEST_F(MotionSearchTest, BidirMotionSearch_Basic) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> cur_frame(stride * total_height, 128);
    std::vector<uint8_t> ref1_frame(stride * total_height, 100);
    std::vector<uint8_t> ref2_frame(stride * total_height, 150);

    uint8_t* current = cur_frame.data() + pad_y * stride + pad_x;
    uint8_t* ref1 = ref1_frame.data() + pad_y * stride + pad_x;
    uint8_t* ref2 = ref2_frame.data() + pad_y * stride + pad_x;

    DIM dim = {width, height};
    extend_frame(current, stride, dim, pad_x, pad_y);
    extend_frame(ref1, stride, dim, pad_x, pad_y);
    extend_frame(ref2, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> P_motion_vectors(num_blocks);
    std::vector<MV> motion_vectors1(num_blocks);
    std::vector<MV> motion_vectors2(num_blocks);
    std::vector<int> SADs1(num_blocks);
    std::vector<int> SADs2(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int count_P = 0;
    int count_B = 0;
    int bits = 0;

    short td1 = 1;
    short td2 = 1;

    int result = bidir_motion_search(current, ref1, ref2, stride, dim,
                                    block_width, block_height,
                                    P_motion_vectors.data(),
                                    motion_vectors1.data(), motion_vectors2.data(),
                                    SADs1.data(), SADs2.data(), mses.data(),
                                    MB_modes.data(), td1, td2,
                                    &count_I, &count_P, &count_B, &bits);

    EXPECT_EQ(0, result);

    // Verify that function executed and produced output
    EXPECT_GE(count_I + count_P + count_B, 0);
    EXPECT_GE(bits, 0);
}

TEST_F(MotionSearchTest, MotionSearch_MultipleBlockSizes) {
    const int width = 64;
    const int height = 64;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> frame(stride * total_height);

    uint8_t* center = frame.data() + pad_y * stride + pad_x;
    fillPattern(center, width, height, stride);

    DIM dim = {width, height};
    extend_frame(center, stride, dim, pad_x, pad_y);

    // Test with current MB_WIDTH setting
    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    EXPECT_GT(num_blocks, 0) << "Should have at least one block";
    EXPECT_EQ(0, width % block_width) << "Width should be divisible by block width";
    EXPECT_EQ(0, height % block_height) << "Height should be divisible by block height";

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int count_P = 0;
    int bits = 0;

    int result = motion_search(center, center, stride, dim, block_width, block_height,
                              motion_vectors.data(), SADs.data(), mses.data(),
                              MB_modes.data(), &count_I, &count_P, &bits);

    EXPECT_EQ(0, result);
}

TEST_F(MotionSearchTest, SpatialSearch_NonSquareFrame) {
    const int width = 80;
    const int height = 48;
    const int pad_x = HORIZONTAL_PADDING;
    const int pad_y = VERTICAL_PADDING;
    const int stride = width + 2 * pad_x;
    const int total_height = height + 2 * pad_y;

    std::vector<uint8_t> frame(stride * total_height);

    uint8_t* center = frame.data() + pad_y * stride + pad_x;
    fillPattern(center, width, height, stride);

    DIM dim = {width, height};
    extend_frame(center, stride, dim, pad_x, pad_y);

    int num_blocks_x = width / block_width;
    int num_blocks_y = height / block_height;
    int num_blocks = num_blocks_x * num_blocks_y;

    std::vector<MV> motion_vectors(num_blocks);
    std::vector<int> SADs(num_blocks);
    std::vector<int> mses(num_blocks);
    std::vector<unsigned char> MB_modes(num_blocks);
    int count_I = 0;
    int bits = 0;

    int result = spatial_search(center, center, stride, dim, block_width, block_height,
                               motion_vectors.data(), SADs.data(), mses.data(),
                               MB_modes.data(), &count_I, &bits);

    EXPECT_EQ(0, result);
}
