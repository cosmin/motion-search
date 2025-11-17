/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "OutputWriter.h"

namespace motion_search {

/**
 * @brief JSON output writer
 *
 * Writes analysis results in JSON format with rich metadata.
 * Supports both frame-level and GOP-level detail.
 */
class JSONWriter : public OutputWriter {
public:
  JSONWriter(std::ostream &out, DetailLevel detail_level);
  ~JSONWriter() override = default;

  void write(const AnalysisResults &results) override;
};

} // namespace motion_search
