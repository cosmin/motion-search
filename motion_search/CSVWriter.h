/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "OutputWriter.h"

namespace motion_search {

/**
 * @brief CSV output writer
 *
 * Provides backward-compatible CSV output format.
 * Supports both frame-level and GOP-level detail.
 */
class CSVWriter : public OutputWriter {
public:
  CSVWriter(std::ostream &out, DetailLevel detail_level);
  ~CSVWriter() override = default;

  void write(const AnalysisResults &results) override;

private:
  void writeFrameHeader();
  void writeGOPHeader();
  void writeFrameData(const FrameData &frame);
  void writeGOPData(const GOPData &gop);
};

} // namespace motion_search
