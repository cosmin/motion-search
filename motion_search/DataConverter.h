/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ComplexityAnalyzer.h"
#include "OutputData.h"

#include <vector>

namespace motion_search {

/**
 * @brief Convert legacy complexity_info_t data to new AnalysisResults format
 */
class DataConverter {
public:
  /**
   * @brief Convert complexity info vector to AnalysisResults
   * @param info_vec Vector of complexity_info_t pointers
   * @param width Video width
   * @param height Video height
   * @param gop_size GOP size
   * @param bframes Number of B-frames
   * @param input_format Input format string (e.g., "y4m", "yuv")
   * @param input_filename Input filename
   * @return AnalysisResults structure
   */
  static AnalysisResults
  convert(const std::vector<complexity_info_t *> &info_vec, int width,
          int height, int gop_size, int bframes,
          const std::string &input_format, const std::string &input_filename);

private:
  static FrameData convertFrame(const complexity_info_t *info, int width,
                                int height);
  static void computeGOPData(AnalysisResults &results, int gop_size);
};

} // namespace motion_search
