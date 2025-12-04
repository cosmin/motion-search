/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "DataConverter.h"

#include <chrono>

namespace motion_search {

FrameData DataConverter::convertFrame(const complexity_info_t *info, int width,
                                      int height) {
  FrameData frame;

  frame.frame_num = info->picNum;
  frame.type = charToFrameType(static_cast<char>(info->picType));

  frame.count_intra = info->count_I;
  frame.count_inter_p = info->count_P;
  frame.count_inter_b = info->count_B;

  frame.estimated_bits = info->bits;
  frame.error = info->error;

  // Compute total blocks for MV stats
  int total_blocks =
      frame.count_intra + frame.count_inter_p + frame.count_inter_b;

  // Phase 4: Use actual computed complexity metrics
  frame.complexity.spatial_variance = info->spatial_variance;
  frame.complexity.motion_magnitude = info->motion_magnitude;
  frame.complexity.ac_energy = info->ac_energy;
  frame.complexity.error_mse = static_cast<double>(info->error);

  // Normalized metrics
  frame.complexity.norm_spatial = info->norm_spatial;
  frame.complexity.norm_motion = info->norm_motion;
  frame.complexity.norm_residual = info->norm_residual;
  frame.complexity.norm_error = info->norm_error;

  // Derived metrics
  frame.complexity.bits_per_pixel = info->bits_per_pixel;

  // Unified scores
  frame.complexity.unified_score_v1 = info->unified_score_v1;
  frame.complexity.unified_score_v2 = info->unified_score_v2;

  // Phase 2 legacy compatibility fields
  frame.complexity.spatial_complexity = info->spatial_variance;
  frame.complexity.motion_complexity = info->motion_magnitude;
  frame.complexity.residual_complexity = static_cast<double>(info->ac_energy);
  frame.complexity.unified_complexity = info->unified_score_v2;

  // MV stats: Not available in current data, use defaults
  frame.mv_stats.mean_magnitude = 0.0;
  frame.mv_stats.max_magnitude = 0.0;
  frame.mv_stats.zero_mv_count = 0;
  frame.mv_stats.total_mv_count = total_blocks;

  return frame;
}

void DataConverter::computeGOPData(AnalysisResults &results, int gop_size) {
  results.gops.clear();

  if (results.frames.empty()) {
    return;
  }

  int current_gop_num = 0;
  int current_gop_start = 0;

  for (size_t i = 0; i < results.frames.size(); ++i) {
    // Start a new GOP when we see an I-frame or reach GOP size
    bool is_i_frame = results.frames[i].type == FrameType::I;
    bool gop_size_reached =
        (i > 0) && ((results.frames[i].frame_num -
                     results.frames[current_gop_start].frame_num) >= gop_size);

    if (is_i_frame && i > 0) {
      // Finish current GOP
      GOPData gop;
      gop.gop_num = current_gop_num;
      gop.start_frame = results.frames[current_gop_start].frame_num;
      gop.end_frame = results.frames[i - 1].frame_num;

      // Accumulate stats
      for (size_t j = current_gop_start; j < i; ++j) {
        gop.total_bits += results.frames[j].estimated_bits;
        gop.avg_complexity += results.frames[j].complexity.unified_score_v2;

        if (results.frames[j].type == FrameType::I)
          gop.i_frame_count++;
        else if (results.frames[j].type == FrameType::P)
          gop.p_frame_count++;
        else if (results.frames[j].type == FrameType::B)
          gop.b_frame_count++;

        // Add frame reference to GOP (for frame-level detail)
        gop.frames.push_back(results.frames[j]);
      }

      // Average complexity
      int frame_count = i - current_gop_start;
      if (frame_count > 0) {
        gop.avg_complexity /= frame_count;
      }

      results.gops.push_back(gop);

      // Start new GOP
      current_gop_start = i;
      current_gop_num++;
    }
  }

  // Add final GOP
  GOPData gop;
  gop.gop_num = current_gop_num;
  gop.start_frame = results.frames[current_gop_start].frame_num;
  gop.end_frame = results.frames.back().frame_num;

  for (size_t j = current_gop_start; j < results.frames.size(); ++j) {
    gop.total_bits += results.frames[j].estimated_bits;
    gop.avg_complexity += results.frames[j].complexity.unified_score_v2;

    if (results.frames[j].type == FrameType::I)
      gop.i_frame_count++;
    else if (results.frames[j].type == FrameType::P)
      gop.p_frame_count++;
    else if (results.frames[j].type == FrameType::B)
      gop.b_frame_count++;

    gop.frames.push_back(results.frames[j]);
  }

  int frame_count = results.frames.size() - current_gop_start;
  if (frame_count > 0) {
    gop.avg_complexity /= frame_count;
  }

  results.gops.push_back(gop);
}

AnalysisResults
DataConverter::convert(const std::vector<complexity_info_t *> &info_vec,
                       int width, int height, int gop_size, int bframes,
                       const std::string &input_format,
                       const std::string &input_filename) {
  AnalysisResults results;

  // Fill metadata
  results.metadata.width = width;
  results.metadata.height = height;
  results.metadata.total_frames = info_vec.size();
  results.metadata.gop_size = gop_size;
  results.metadata.bframes = bframes;
  results.metadata.input_format = input_format;
  results.metadata.input_filename = input_filename;
  results.metadata.analysis_time = std::chrono::system_clock::now();

  // Convert each frame
  results.frames.reserve(info_vec.size());
  for (const auto *info : info_vec) {
    results.frames.push_back(convertFrame(info, width, height));
  }

  // Compute GOP data
  computeGOPData(results, gop_size);

  return results;
}

} // namespace motion_search
