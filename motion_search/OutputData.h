/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace motion_search {

/**
 * @brief Frame type enumeration
 */
enum class FrameType { I, P, B, UNKNOWN };

/**
 * @brief Convert FrameType to string
 */
inline std::string frameTypeToString(FrameType type) {
  switch (type) {
  case FrameType::I:
    return "I";
  case FrameType::P:
    return "P";
  case FrameType::B:
    return "B";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Convert char to FrameType
 */
inline FrameType charToFrameType(char c) {
  switch (c) {
  case 'I':
    return FrameType::I;
  case 'P':
    return FrameType::P;
  case 'B':
    return FrameType::B;
  default:
    return FrameType::UNKNOWN;
  }
}

/**
 * @brief Motion vector statistics for a frame
 */
struct MVStats {
  double mean_magnitude = 0.0;
  double max_magnitude = 0.0;
  int zero_mv_count = 0;
  int total_mv_count = 0;
};

/**
 * @brief Complexity metrics for a frame
 */
struct ComplexityMetrics {
  double spatial_complexity = 0.0;  // Variance-based
  double motion_complexity = 0.0;   // Motion vector magnitude
  double residual_complexity = 0.0; // AC energy
  double error_mse = 0.0;           // Reconstruction error (MSE)
  double unified_complexity = 0.0;  // Combined score (Phase 4)
};

/**
 * @brief Data for a single frame
 */
struct FrameData {
  int frame_num = 0;
  FrameType type = FrameType::UNKNOWN;

  // Block mode distribution
  int count_intra = 0;
  int count_inter_p = 0;
  int count_inter_b = 0;

  // Bit estimation
  int64_t estimated_bits = 0;

  // Error metric
  int64_t error = 0; // MSE or similar

  // Complexity metrics
  ComplexityMetrics complexity;

  // Motion vector statistics
  MVStats mv_stats;

  // Per-macroblock data (optional, for JSON/XML only)
  // std::vector<MacroblockData> macroblocks;  // Future enhancement
};

/**
 * @brief Data for a GOP (Group of Pictures)
 */
struct GOPData {
  int gop_num = 0;
  int start_frame = 0;
  int end_frame = 0;
  int64_t total_bits = 0;
  double avg_complexity = 0.0;
  int i_frame_count = 0;
  int p_frame_count = 0;
  int b_frame_count = 0;

  // Frames in this GOP (only populated if detail_level == "frame")
  std::vector<FrameData> frames;
};

/**
 * @brief Metadata about the video and analysis parameters
 */
struct VideoMetadata {
  int width = 0;
  int height = 0;
  int total_frames = 0;
  int gop_size = 0;
  int bframes = 0;
  std::string input_format; // "y4m", "yuv", etc.
  std::string input_filename;
  std::chrono::system_clock::time_point analysis_time;
  std::string version = "2.0.0"; // Version of output format
};

/**
 * @brief Complete analysis results
 */
struct AnalysisResults {
  VideoMetadata metadata;
  std::vector<GOPData> gops;
  std::vector<FrameData> frames; // All frames (populated if detail_level ==
                                 // "frame")
};

} // namespace motion_search
