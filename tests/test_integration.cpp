/*
 * Integration tests for motion_search
 * Tests end-to-end video complexity analysis with simplified API usage
 */

#include <gtest/gtest.h>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <cstdio>

#include "ComplexityAnalyzer.h"
#include "YUVSequenceReader.h"
#include "Y4MSequenceReader.h"
#include "common.h"

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_data_dir = TEST_DATA_DIR;
    }

    bool fileExists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    unique_file_t openFile(const std::string& path) {
        FILE* fp = fopen(path.c_str(), "rb");
        return unique_file_t(fp);
    }

    std::string test_data_dir;
};

TEST_F(IntegrationTest, YUVReader_OpenFile) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file
                     << ". Run generate_test_videos.sh first.";
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);

    bool opened = reader.Open(std::move(file), test_file, dim);
    EXPECT_TRUE(opened) << "Failed to open YUV file";
    EXPECT_TRUE(reader.isOpen());
    EXPECT_EQ(320, reader.dim().width);
    EXPECT_EQ(180, reader.dim().height);
}

TEST_F(IntegrationTest, Y4MReader_OpenFile) {
    std::string test_file = test_data_dir + "/testsrc.y4m";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file
                     << ". Run generate_test_videos.sh first.";
    }

    Y4MSequenceReader reader;
    auto file = openFile(test_file);

    bool opened = reader.Open(std::move(file), test_file);
    EXPECT_TRUE(opened) << "Failed to open Y4M file";
    EXPECT_TRUE(reader.isOpen());
    // Y4M should parse dimensions from file
    EXPECT_GT(reader.dim().width, 0);
    EXPECT_GT(reader.dim().height, 0);
}

TEST_F(IntegrationTest, ComplexityAnalyzer_BasicAnalysis) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file
                     << ". Run generate_test_videos.sh first.";
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);
    reader.Open(std::move(file), test_file, dim);

    ComplexityAnalyzer analyzer(&reader, 150, 10, 0);

    ASSERT_NO_THROW(analyzer.analyze()) << "Analysis should not throw";

    auto info = analyzer.getInfo();
    EXPECT_GT(info.size(), 0) << "ComplexityAnalyzer should produce output";

    // First frame should be I-frame
    if (info.size() > 0) {
        EXPECT_EQ('I', info[0]->picType) << "First frame should be I-frame";
        EXPECT_EQ(0, info[0]->picNum) << "First frame number should be 0";
    }
}

TEST_F(IntegrationTest, ComplexityAnalyzer_FrameTypes) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);
    reader.Open(std::move(file), test_file, dim);

    // Analyze without B-frames
    ComplexityAnalyzer analyzer(&reader, 150, 10, 0);
    analyzer.analyze();

    auto info = analyzer.getInfo();
    ASSERT_GT(info.size(), 0);

    // Verify frame types
    for (size_t i = 0; i < info.size(); i++) {
        EXPECT_TRUE(info[i]->picType == 'I' || info[i]->picType == 'P' || info[i]->picType == 'B')
            << "Invalid picture type at frame " << i << ": " << info[i]->picType;
    }

    // With no B-frames, we should only see I and P frames
    bool has_b_frame = false;
    for (size_t i = 0; i < info.size(); i++) {
        if (info[i]->picType == 'B') {
            has_b_frame = true;
            break;
        }
    }
    EXPECT_FALSE(has_b_frame) << "Should not have B-frames when b_frames=0";
}

TEST_F(IntegrationTest, ComplexityAnalyzer_WithBFrames) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);
    reader.Open(std::move(file), test_file, dim);

    // Analyze with B-frames
    ComplexityAnalyzer analyzer(&reader, 150, 10, 1);
    analyzer.analyze();

    auto info = analyzer.getInfo();
    ASSERT_GT(info.size(), 0);

    // Should have at least some B-frames
    bool has_i_frame = false;
    bool has_p_frame = false;
    bool has_b_frame = false;

    for (size_t i = 0; i < info.size(); i++) {
        if (info[i]->picType == 'I') has_i_frame = true;
        if (info[i]->picType == 'P') has_p_frame = true;
        if (info[i]->picType == 'B') has_b_frame = true;
    }

    EXPECT_TRUE(has_i_frame) << "Should have I-frames";
    EXPECT_TRUE(has_p_frame) << "Should have P-frames";
    EXPECT_TRUE(has_b_frame) << "Should have B-frames when b_frames=1";
}

TEST_F(IntegrationTest, ComplexityAnalyzer_OutputValidity) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);
    reader.Open(std::move(file), test_file, dim);

    ComplexityAnalyzer analyzer(&reader, 150, 10, 0);
    analyzer.analyze();

    auto info = analyzer.getInfo();

    for (size_t i = 0; i < info.size(); i++) {
        // Verify all output fields are reasonable
        EXPECT_GE(info[i]->picNum, 0) << "Picture number should be non-negative";
        EXPECT_GE(info[i]->bits, 0) << "Bits should be non-negative";
        EXPECT_GE(info[i]->error, 0) << "Error should be non-negative";
        EXPECT_GE(info[i]->count_I, 0) << "I-block count should be non-negative";
        EXPECT_GE(info[i]->count_P, 0) << "P-block count should be non-negative";
        EXPECT_GE(info[i]->count_B, 0) << "B-block count should be non-negative";
    }
}

TEST_F(IntegrationTest, ComplexityAnalyzer_ConsistentResults) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }

    DIM dim = {320, 180};

    // Run analysis twice
    YUVSequenceReader reader1;
    auto file1 = openFile(test_file);
    reader1.Open(std::move(file1), test_file, dim);
    ComplexityAnalyzer analyzer1(&reader1, 150, 10, 0);
    analyzer1.analyze();
    auto info1 = analyzer1.getInfo();

    YUVSequenceReader reader2;
    auto file2 = openFile(test_file);
    reader2.Open(std::move(file2), test_file, dim);
    ComplexityAnalyzer analyzer2(&reader2, 150, 10, 0);
    analyzer2.analyze();
    auto info2 = analyzer2.getInfo();

    // Results should be identical
    ASSERT_EQ(info1.size(), info2.size()) << "Both runs should analyze same number of frames";

    for (size_t i = 0; i < info1.size(); i++) {
        EXPECT_EQ(info1[i]->picNum, info2[i]->picNum);
        EXPECT_EQ(info1[i]->picType, info2[i]->picType);
        EXPECT_EQ(info1[i]->error, info2[i]->error);
        EXPECT_EQ(info1[i]->bits, info2[i]->bits);
        EXPECT_EQ(info1[i]->count_I, info2[i]->count_I);
        EXPECT_EQ(info1[i]->count_P, info2[i]->count_P);
        EXPECT_EQ(info1[i]->count_B, info2[i]->count_B);
    }
}

TEST_F(IntegrationTest, ComplexityAnalyzer_SmallGOP) {
    std::string test_file = test_data_dir + "/testsrc.yuv";

    if (!fileExists(test_file)) {
        GTEST_SKIP() << "Test file not found: " << test_file;
    }

    DIM dim = {320, 180};
    YUVSequenceReader reader;
    auto file = openFile(test_file);
    reader.Open(std::move(file), test_file, dim);

    // Use small GOP size
    int gop_size = 5;
    ComplexityAnalyzer analyzer(&reader, gop_size, 10, 0);
    analyzer.analyze();

    auto info = analyzer.getInfo();
    ASSERT_EQ(10, info.size()) << "Should analyze all 10 frames";

    // First frame should be I-frame
    EXPECT_EQ('I', info[0]->picType);

    // Frame at GOP boundary should be I-frame
    if (info.size() > 5) {
        EXPECT_EQ('I', info[5]->picType) << "Frame at GOP boundary should be I-frame";
    }
}
