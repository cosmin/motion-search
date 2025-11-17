/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "XMLWriter.h"

#include <sstream>
#include <tinyxml2.h>

using namespace tinyxml2;

namespace motion_search {

XMLWriter::XMLWriter(std::ostream &out, DetailLevel detail_level)
    : OutputWriter(out, detail_level) {}

void XMLWriter::write(const AnalysisResults &results) {
  XMLDocument doc;

  // XML declaration
  XMLDeclaration *decl = doc.NewDeclaration();
  doc.InsertFirstChild(decl);

  // Root element
  XMLElement *root = doc.NewElement("motion_analysis");
  root->SetAttribute("version", results.metadata.version.c_str());
  doc.InsertEndChild(root);

  // Metadata
  XMLElement *metadata = doc.NewElement("metadata");
  root->InsertEndChild(metadata);

  XMLElement *video = doc.NewElement("video");
  video->SetAttribute("width", results.metadata.width);
  video->SetAttribute("height", results.metadata.height);
  video->SetAttribute("frames", results.metadata.total_frames);
  metadata->InsertEndChild(video);

  XMLElement *encoding = doc.NewElement("encoding");
  encoding->SetAttribute("gop_size", results.metadata.gop_size);
  encoding->SetAttribute("bframes", results.metadata.bframes);
  metadata->InsertEndChild(encoding);

  XMLElement *input = doc.NewElement("input");
  input->SetAttribute("format", results.metadata.input_format.c_str());
  input->SetAttribute("filename", results.metadata.input_filename.c_str());
  metadata->InsertEndChild(input);

  // Format timestamp
  auto time_t_val =
      std::chrono::system_clock::to_time_t(results.metadata.analysis_time);
  char time_str[100];
  std::strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%SZ",
                std::gmtime(&time_t_val));

  XMLElement *timestamp = doc.NewElement("timestamp");
  timestamp->SetText(time_str);
  metadata->InsertEndChild(timestamp);

  // GOPs
  XMLElement *gops = doc.NewElement("gops");
  root->InsertEndChild(gops);

  for (const auto &gop : results.gops) {
    XMLElement *gop_elem = doc.NewElement("gop");
    gop_elem->SetAttribute("num", gop.gop_num);
    gop_elem->SetAttribute("start", gop.start_frame);
    gop_elem->SetAttribute("end", gop.end_frame);
    gop_elem->SetAttribute("total_bits", (int64_t)gop.total_bits);
    gop_elem->SetAttribute("avg_complexity", gop.avg_complexity);
    gop_elem->SetAttribute("i_frames", gop.i_frame_count);
    gop_elem->SetAttribute("p_frames", gop.p_frame_count);
    gop_elem->SetAttribute("b_frames", gop.b_frame_count);
    gops->InsertEndChild(gop_elem);

    // Add frames if detail level is FRAME
    if (detail_level_ == DetailLevel::FRAME && !gop.frames.empty()) {
      for (const auto &frame : gop.frames) {
        XMLElement *frame_elem = doc.NewElement("frame");
        frame_elem->SetAttribute("num", frame.frame_num);
        frame_elem->SetAttribute("type", frameTypeToString(frame.type).c_str());
        gop_elem->InsertEndChild(frame_elem);

        // Complexity
        XMLElement *complexity = doc.NewElement("complexity");
        complexity->SetAttribute("spatial",
                                 frame.complexity.spatial_complexity);
        complexity->SetAttribute("motion", frame.complexity.motion_complexity);
        complexity->SetAttribute("residual",
                                 frame.complexity.residual_complexity);
        complexity->SetAttribute("error_mse", frame.complexity.error_mse);
        complexity->SetAttribute("unified",
                                 frame.complexity.unified_complexity);
        frame_elem->InsertEndChild(complexity);

        // Block modes
        XMLElement *block_modes = doc.NewElement("block_modes");
        block_modes->SetAttribute("intra", frame.count_intra);
        block_modes->SetAttribute("inter_p", frame.count_inter_p);
        block_modes->SetAttribute("inter_b", frame.count_inter_b);
        frame_elem->InsertEndChild(block_modes);

        // Error
        XMLElement *error = doc.NewElement("error");
        error->SetAttribute("value", (int64_t)frame.error);
        frame_elem->InsertEndChild(error);

        // Bits
        XMLElement *bits = doc.NewElement("bits");
        bits->SetAttribute("estimated", (int64_t)frame.estimated_bits);
        frame_elem->InsertEndChild(bits);

        // MV stats
        XMLElement *mv_stats = doc.NewElement("mv_stats");
        mv_stats->SetAttribute("mean_magnitude", frame.mv_stats.mean_magnitude);
        mv_stats->SetAttribute("max_magnitude", frame.mv_stats.max_magnitude);
        mv_stats->SetAttribute("zero_count", frame.mv_stats.zero_mv_count);
        mv_stats->SetAttribute("total_count", frame.mv_stats.total_mv_count);
        frame_elem->InsertEndChild(mv_stats);
      }
    }
  }

  // Print to string and write to output stream
  XMLPrinter printer;
  doc.Print(&printer);
  out_ << printer.CStr() << std::endl;
}

} // namespace motion_search
