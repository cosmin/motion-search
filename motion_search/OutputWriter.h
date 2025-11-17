/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "OutputData.h"

#include <memory>
#include <ostream>
#include <string>

namespace motion_search {

/**
 * @brief Detail level for output
 */
enum class DetailLevel {
  FRAME, // Per-frame output
  GOP    // Per-GOP output only
};

/**
 * @brief Convert string to DetailLevel
 */
inline DetailLevel stringToDetailLevel(const std::string &level) {
  if (level == "frame") {
    return DetailLevel::FRAME;
  } else if (level == "gop") {
    return DetailLevel::GOP;
  } else {
    throw std::invalid_argument("Unknown detail level: " + level +
                                ". Valid options: frame, gop");
  }
}

/**
 * @brief Abstract interface for output writers
 */
class OutputWriter {
public:
  virtual ~OutputWriter() = default;

  /**
   * @brief Write the analysis results
   * @param results The complete analysis results
   */
  virtual void write(const AnalysisResults &results) = 0;

protected:
  OutputWriter(std::ostream &out, DetailLevel detail_level)
      : out_(out), detail_level_(detail_level) {}

  std::ostream &out_;
  DetailLevel detail_level_;
};

/**
 * @brief Factory function to create an OutputWriter
 * @param format Output format (csv, json, xml)
 * @param detail_level Detail level (frame, gop)
 * @param out Output stream
 * @return Unique pointer to the created OutputWriter
 */
std::unique_ptr<OutputWriter> createOutputWriter(const std::string &format,
                                                 DetailLevel detail_level,
                                                 std::ostream &out);

} // namespace motion_search
