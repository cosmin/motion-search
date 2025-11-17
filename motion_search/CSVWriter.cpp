/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "CSVWriter.h"

#include <iomanip>

namespace motion_search {

CSVWriter::CSVWriter(std::ostream &out, DetailLevel detail_level)
    : OutputWriter(out, detail_level) {}

void CSVWriter::writeFrameHeader() {
  out_ << "picNum,picType,count_I,count_P,count_B,error,bits\n";
}

void CSVWriter::writeGOPHeader() {
  out_ << "gop,frames,total_bits,avg_complexity,i_frames,p_frames,b_frames\n";
}

void CSVWriter::writeFrameData(const FrameData &frame) {
  out_ << frame.frame_num << "," << frameTypeToString(frame.type) << ","
       << frame.count_intra << "," << frame.count_inter_p << ","
       << frame.count_inter_b << "," << frame.error << ","
       << frame.estimated_bits << "\n";
}

void CSVWriter::writeGOPData(const GOPData &gop) {
  int frame_count = gop.end_frame - gop.start_frame + 1;
  out_ << gop.gop_num << "," << frame_count << "," << gop.total_bits << ","
       << std::fixed << std::setprecision(2) << gop.avg_complexity << ","
       << gop.i_frame_count << "," << gop.p_frame_count << ","
       << gop.b_frame_count << "\n";
}

void CSVWriter::write(const AnalysisResults &results) {
  if (detail_level_ == DetailLevel::FRAME) {
    // Frame-level output (backward compatible)
    writeFrameHeader();
    for (const auto &frame : results.frames) {
      writeFrameData(frame);
    }
  } else {
    // GOP-level output
    writeGOPHeader();
    for (const auto &gop : results.gops) {
      writeGOPData(gop);
    }
  }
}

} // namespace motion_search
