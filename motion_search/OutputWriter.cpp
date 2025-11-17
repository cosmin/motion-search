/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "OutputWriter.h"

#include "CSVWriter.h"
#include "JSONWriter.h"
#include "XMLWriter.h"

#include <stdexcept>

namespace motion_search {

std::unique_ptr<OutputWriter> createOutputWriter(const std::string &format,
                                                 DetailLevel detail_level,
                                                 std::ostream &out) {
  if (format == "csv") {
    return std::make_unique<CSVWriter>(out, detail_level);
  } else if (format == "json") {
    return std::make_unique<JSONWriter>(out, detail_level);
  } else if (format == "xml") {
    return std::make_unique<XMLWriter>(out, detail_level);
  } else {
    throw std::invalid_argument("Unknown output format: " + format +
                                ". Valid formats: csv, json, xml");
  }
}

} // namespace motion_search
