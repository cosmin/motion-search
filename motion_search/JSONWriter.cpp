/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "JSONWriter.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace motion_search {

JSONWriter::JSONWriter(std::ostream &out, DetailLevel detail_level)
    : OutputWriter(out, detail_level) {}

void JSONWriter::write(const AnalysisResults &results) {
  json j;

  // Write metadata
  auto time_t_val =
      std::chrono::system_clock::to_time_t(results.metadata.analysis_time);
  char time_str[100];
  std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&time_t_val));

  j["metadata"] = {{"width", results.metadata.width},
                   {"height", results.metadata.height},
                   {"frames", results.metadata.total_frames},
                   {"gop_size", results.metadata.gop_size},
                   {"bframes", results.metadata.bframes},
                   {"input_format", results.metadata.input_format},
                   {"input_filename", results.metadata.input_filename},
                   {"analysis_timestamp", time_str},
                   {"version", results.metadata.version}};

  // Write GOPs
  j["gops"] = json::array();
  for (const auto &gop : results.gops) {
    json gop_obj = {
        {"gop_num", gop.gop_num},
        {"start_frame", gop.start_frame},
        {"end_frame", gop.end_frame},
        {"total_bits", gop.total_bits},
        {"avg_complexity", gop.avg_complexity},
        {"i_frame_count", gop.i_frame_count},
        {"p_frame_count", gop.p_frame_count},
        {"b_frame_count", gop.b_frame_count},
    };

    // Add frames if detail level is FRAME
    if (detail_level_ == DetailLevel::FRAME && !gop.frames.empty()) {
      gop_obj["frames"] = json::array();
      for (const auto &frame : gop.frames) {
        json frame_obj = {
            {"frame_num", frame.frame_num},
            {"type", frameTypeToString(frame.type)},
            {"complexity",
             {
                 {"spatial", frame.complexity.spatial_complexity},
                 {"motion", frame.complexity.motion_complexity},
                 {"residual", frame.complexity.residual_complexity},
                 {"error_mse", frame.complexity.error_mse},
                 {"unified", frame.complexity.unified_complexity},
             }},
            {"block_modes",
             {
                 {"intra", frame.count_intra},
                 {"inter_p", frame.count_inter_p},
                 {"inter_b", frame.count_inter_b},
             }},
            {"error", frame.error},
            {"estimated_bits", frame.estimated_bits},
            {"mv_stats",
             {
                 {"mean_magnitude", frame.mv_stats.mean_magnitude},
                 {"max_magnitude", frame.mv_stats.max_magnitude},
                 {"zero_mv_count", frame.mv_stats.zero_mv_count},
                 {"total_mv_count", frame.mv_stats.total_mv_count},
             }},
        };
        gop_obj["frames"].push_back(frame_obj);
      }
    }

    j["gops"].push_back(gop_obj);
  }

  // Write to output stream with pretty formatting
  out_ << j.dump(2) << std::endl;
}

} // namespace motion_search
