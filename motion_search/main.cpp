
/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "ComplexityAnalyzer.h"
#include "ComplexityNormalization.h"
#include "DataConverter.h"
#include "OutputWriter.h"
#include "Y4MSequenceReader.h"
#include "YUVSequenceReader.h"
#ifdef HAVE_FFMPEG
#include "FFmpegSequenceReader.h"
#endif

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"

// Define command-line flags

// Input options
ABSL_FLAG(std::string, input, "", "Input video file path (required)");
ABSL_FLAG(
    int32_t, width, 0,
    "Video width in pixels (required for raw YUV files, ignored for Y4M)");
ABSL_FLAG(int32_t, height, 0,
          "Video height in pixels (required for raw YUV files, ignored for "
          "Y4M)");

// Analysis options
ABSL_FLAG(int32_t, frames, 0,
          "Number of frames to process (0 = all frames, default: 0)");
ABSL_FLAG(
    int32_t, gop_size, 150,
    "GOP (Group of Pictures) size for encoding simulation (default: 150)");
ABSL_FLAG(int32_t, bframes, 0, "Number of consecutive B-frames (default: 0)");

// Output options
ABSL_FLAG(std::string, output, "",
          "Output file path (required, use '-' for stdout)");
ABSL_FLAG(std::string, format, "csv",
          "Output format: csv, json, xml (default: csv)");
ABSL_FLAG(std::string, detail, "frame",
          "Detail level: frame (per-frame data), gop (per-GOP data, default: "
          "frame)");

// Input format options (Phase 3)
ABSL_FLAG(bool, use_ffmpeg, false,
          "Use FFmpeg for input decoding (supports MP4, MKV, AVI, WebM, etc.)");

// Phase 4: Complexity scoring options
ABSL_FLAG(std::string, complexity_score, "v2",
          "Unified complexity score version: v1 (bpp-based), v2 (weighted, "
          "default)");
ABSL_FLAG(double, weight_spatial, 0.25,
          "Weight for spatial complexity in v2 scoring (default: 0.25)");
ABSL_FLAG(double, weight_motion, 0.30,
          "Weight for motion complexity in v2 scoring (default: 0.30)");
ABSL_FLAG(double, weight_residual, 0.25,
          "Weight for residual complexity in v2 scoring (default: 0.25)");
ABSL_FLAG(double, weight_error, 0.20,
          "Weight for error complexity in v2 scoring (default: 0.20)");

// Legacy support flags (mapped from old parser)
ABSL_FLAG(int32_t, W, 0, "Legacy: same as --width");
ABSL_FLAG(int32_t, H, 0, "Legacy: same as --height");
ABSL_FLAG(int32_t, n, 0, "Legacy: same as --frames");
ABSL_FLAG(int32_t, g, 0, "Legacy: same as --gop_size");
ABSL_FLAG(int32_t, b, 0, "Legacy: same as --bframes");

namespace {

struct CTX {
  std::string inputFile;
  std::string outputFile;
  int width = 0;
  int height = 0;
  int num_frames = 0;
  int gop_size = 150;
  int b_frames = 0;
  bool use_ffmpeg = false;
  std::string complexity_score = "v2";
  complexity::ComplexityWeights weights; // Phase 4: Complexity weights
};

std::unique_ptr<IVideoSequenceReader>
getReader(const std::string &filename, const DIM dim, bool use_ffmpeg) {
  std::unique_ptr<IVideoSequenceReader> reader;

#ifdef HAVE_FFMPEG
  if (use_ffmpeg) {
    return std::make_unique<FFmpegSequenceReader>(filename);
  }
#else
  if (use_ffmpeg) {
    std::cerr
        << "Error: FFmpeg support not compiled in. Rebuild with "
           "-DENABLE_FFMPEG=ON\n";
    return nullptr;
  }
#endif

  const size_t pos = filename.find_last_of('.');

  if (std::string::npos != pos) {
    std::string ext = filename.substr(pos);

    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](int c) { return (char)::tolower(c); });

    unique_file_t file(fopen(filename.c_str(), "rb"));
    if (!file) {
      return nullptr;
    }

    if (ext.compare(".yuv") == 0) {
      std::unique_ptr<YUVSequenceReader> p(new YUVSequenceReader());
      if (p) {
        p->Open(std::move(file), filename, dim);
        reader = std::move(p);
      }
    } else if (ext.compare(".y4m") == 0) {
      std::unique_ptr<Y4MSequenceReader> p(new Y4MSequenceReader);
      if (p) {
        p->Open(std::move(file), filename);
        reader = std::move(p);
      }
    }
  }

  if ((!reader) || !reader->isOpen()) {
    return nullptr;
  }

  return reader;
}

void ParseAndValidateFlags(CTX &ctx,
                           const std::vector<std::string> &positional_args) {
  // Handle positional arguments (backward compatibility)
  if (positional_args.size() >= 1) {
    ctx.inputFile = positional_args[0];
  }
  if (positional_args.size() >= 2) {
    ctx.outputFile = positional_args[1];
  }

  // Check for explicit flags (new style)
  std::string input_flag = absl::GetFlag(FLAGS_input);
  std::string output_flag = absl::GetFlag(FLAGS_output);

  // Prefer explicit flags over positional arguments
  if (!input_flag.empty()) {
    ctx.inputFile = input_flag;
  }
  if (!output_flag.empty()) {
    ctx.outputFile = output_flag;
  }

  // Validate required arguments
  if (ctx.inputFile.empty()) {
    std::cerr << "Error: Input file is required\n";
    std::cerr << "Use --input=<file> or provide as first positional argument\n";
    exit(1);
  }

  if (ctx.outputFile.empty()) {
    std::cerr << "Error: Output file is required\n";
    std::cerr << "Use --output=<file> or provide as second positional "
                 "argument\n";
    exit(1);
  }

  // Handle width/height with legacy support
  ctx.width = absl::GetFlag(FLAGS_width);
  ctx.height = absl::GetFlag(FLAGS_height);

  // Check legacy flags
  int legacy_width = absl::GetFlag(FLAGS_W);
  int legacy_height = absl::GetFlag(FLAGS_H);

  if (legacy_width > 0) {
    if (ctx.width > 0 && ctx.width != legacy_width) {
      std::cerr << "Warning: Both -W and --width specified, using --width\n";
    } else {
      ctx.width = legacy_width;
    }
  }

  if (legacy_height > 0) {
    if (ctx.height > 0 && ctx.height != legacy_height) {
      std::cerr << "Warning: Both -H and --height specified, using --height\n";
    } else {
      ctx.height = legacy_height;
    }
  }

  // Handle num_frames with legacy support
  ctx.num_frames = absl::GetFlag(FLAGS_frames);
  int legacy_frames = absl::GetFlag(FLAGS_n);
  if (legacy_frames > 0) {
    if (ctx.num_frames > 0 && ctx.num_frames != legacy_frames) {
      std::cerr << "Warning: Both -n and --frames specified, using --frames\n";
    } else {
      ctx.num_frames = legacy_frames;
    }
  }

  // Handle GOP size with legacy support
  ctx.gop_size = absl::GetFlag(FLAGS_gop_size);
  int legacy_gop = absl::GetFlag(FLAGS_g);
  if (legacy_gop > 0) {
    if (ctx.gop_size != 150 && ctx.gop_size != legacy_gop) {
      std::cerr
          << "Warning: Both -g and --gop_size specified, using --gop_size\n";
    } else {
      ctx.gop_size = legacy_gop;
    }
  }

  // Handle B-frames with legacy support
  ctx.b_frames = absl::GetFlag(FLAGS_bframes);
  int legacy_bframes = absl::GetFlag(FLAGS_b);
  if (legacy_bframes > 0) {
    if (ctx.b_frames > 0 && ctx.b_frames != legacy_bframes) {
      std::cerr
          << "Warning: Both -b and --bframes specified, using --bframes\n";
    } else {
      ctx.b_frames = legacy_bframes;
    }
  }

  // Validate GOP size
  if (ctx.gop_size < 1) {
    std::cerr << "Error: Invalid GOP size specified (must be >= 1)\n";
    exit(1);
  }

  // Validate B-frames
  if (ctx.b_frames < 0) {
    std::cerr << "Error: Invalid number of B-frames specified (must be >= 0)\n";
    exit(1);
  }

  // Validate format
  std::string format = absl::GetFlag(FLAGS_format);
  if (format != "csv" && format != "json" && format != "xml") {
    std::cerr << "Error: Invalid output format '" << format << "'\n";
    std::cerr << "Supported formats: csv, json, xml\n";
    exit(1);
  }

  // Validate detail level
  std::string detail = absl::GetFlag(FLAGS_detail);
  if (detail != "frame" && detail != "gop") {
    std::cerr << "Error: Invalid detail level '" << detail << "'\n";
    std::cerr << "Supported detail levels: frame, gop\n";
    exit(1);
  }

  // Handle FFmpeg flag
  ctx.use_ffmpeg = absl::GetFlag(FLAGS_use_ffmpeg);

#ifndef HAVE_FFMPEG
  if (ctx.use_ffmpeg) {
    std::cerr << "Error: FFmpeg support not compiled in.\n";
    std::cerr
        << "Please rebuild with -DENABLE_FFMPEG=ON to use --use_ffmpeg flag\n";
    exit(1);
  }
#endif

  // Phase 4: Validate and load complexity scoring configuration
  ctx.complexity_score = absl::GetFlag(FLAGS_complexity_score);
  if (ctx.complexity_score != "v1" && ctx.complexity_score != "v2") {
    std::cerr << "Error: Invalid complexity score version '"
              << ctx.complexity_score << "'\n";
    std::cerr << "Supported versions: v1 (bpp-based), v2 (weighted)\n";
    exit(1);
  }

  // Load weights from flags
  ctx.weights.w_spatial = absl::GetFlag(FLAGS_weight_spatial);
  ctx.weights.w_motion = absl::GetFlag(FLAGS_weight_motion);
  ctx.weights.w_residual = absl::GetFlag(FLAGS_weight_residual);
  ctx.weights.w_error = absl::GetFlag(FLAGS_weight_error);

  // Validate that weights are positive and sum to approximately 1.0
  if (ctx.weights.w_spatial < 0.0 || ctx.weights.w_motion < 0.0 ||
      ctx.weights.w_residual < 0.0 || ctx.weights.w_error < 0.0) {
    std::cerr << "Error: All complexity weights must be non-negative\n";
    exit(1);
  }

  if (!ctx.weights.isValid()) {
    std::cerr << "Warning: Complexity weights do not sum to 1.0 (sum = "
              << ctx.weights.sum() << ")\n";
    std::cerr << "Weights will be used as-is, but results may not be "
                 "normalized correctly\n";
  }
}

} // namespace

int main(int argc, char *argv[]) {
  // Set up usage message
  absl::SetProgramUsageMessage(
      "Motion Search Video Complexity Analyzer\n\n"
      "Analyzes video complexity using motion estimation and spatial "
      "analysis.\n"
      "Supports Y4M, raw YUV, and optionally FFmpeg-decodable formats.\n\n"
      "Usage:\n"
      "  motion_search --input=<file> --output=<file> [options]\n"
      "  motion_search <input_file> <output_file> [options]  (legacy syntax)\n"
      "\n"
      "Examples:\n"
      "  motion_search --input=video.y4m --output=results.csv\n"
      "  motion_search --input=video.yuv --width=1920 --height=1080 "
      "--output=results.csv\n"
      "  motion_search --input=video.mp4 --use_ffmpeg --output=results.json "
      "--format=json\n"
      "  motion_search video.y4m results.csv -g=60 -b=2  (legacy syntax)\n"
      "\n"
      "Required flags:\n"
      "  --input=<file>   Input video file (.y4m, .yuv, or use --use_ffmpeg)\n"
      "  --output=<file>  Output file (use '-' for stdout)\n"
      "\n"
      "Optional flags:\n"
      "  --width=<n>      Video width (required for .yuv files)\n"
      "  --height=<n>     Video height (required for .yuv files)\n"
      "  --frames=<n>     Number of frames to process (0 = all, default: 0)\n"
      "  --gop_size=<n>   GOP size for simulation (default: 150)\n"
      "  --bframes=<n>    Number of consecutive B-frames (default: 0)\n"
      "  --format=<fmt>   Output format: csv, json, xml (default: csv)\n"
      "  --detail=<lvl>   Detail level: frame, gop (default: frame)\n"
      "  --use_ffmpeg     Use FFmpeg for input (supports MP4, MKV, etc.)\n"
      "  --complexity_score=<ver>  Score version: v1, v2 (default: v2)\n"
      "  --weight_spatial=<w>      Spatial weight for v2 (default: 0.25)\n"
      "  --weight_motion=<w>       Motion weight for v2 (default: 0.30)\n"
      "  --weight_residual=<w>     Residual weight for v2 (default: 0.25)\n"
      "  --weight_error=<w>        Error weight for v2 (default: 0.20)\n"
      "\n"
      "Legacy flags (backward compatibility):\n"
      "  -W=<n>           Same as --width\n"
      "  -H=<n>           Same as --height\n"
      "  -n=<n>           Same as --frames\n"
      "  -g=<n>           Same as --gop_size\n"
      "  -b=<n>           Same as --bframes\n");

  // Parse command line
  std::vector<char *> positional = absl::ParseCommandLine(argc, argv);

  // Convert char* vector to string vector (excluding program name)
  std::vector<std::string> positional_args;
  for (size_t i = 1; i < positional.size(); ++i) {
    positional_args.push_back(std::string(positional[i]));
  }

  CTX ctx;
  ParseAndValidateFlags(ctx, positional_args);

  auto reader =
      getReader(ctx.inputFile, {ctx.width, ctx.height}, ctx.use_ffmpeg);
  if (reader == nullptr) {
    std::cerr << "Error: Unsupported input format for " << ctx.inputFile
              << "\n";
    std::cerr << "Supported formats: .y4m, .yuv\n";
    std::cerr << "For .yuv files, you must specify --width and --height\n";
#ifdef HAVE_FFMPEG
    std::cerr << "Or use --use_ffmpeg for MP4, MKV, AVI, WebM, etc.\n";
#endif
    return 1;
  }

  ComplexityAnalyzer analyzer(reader.get(), ctx.gop_size, ctx.num_frames,
                              ctx.b_frames);

  // Phase 4: Set complexity weights
  analyzer.setComplexityWeights(ctx.weights);

  const auto begin = std::chrono::high_resolution_clock::now();
  analyzer.analyze();
  const auto end = std::chrono::high_resolution_clock::now();

  std::cerr << "Input file: '" << ctx.inputFile << "'\n";
  std::cerr << "width: " << reader->dim().width << "\n";
  std::cerr << "height: " << reader->dim().height << "\n";
  std::cerr << "Complexity score version: " << ctx.complexity_score << "\n";
  std::cerr << "Weights: spatial=" << ctx.weights.w_spatial
            << ", motion=" << ctx.weights.w_motion
            << ", residual=" << ctx.weights.w_residual
            << ", error=" << ctx.weights.w_error << "\n";

  // Get analysis results
  vector<complexity_info_t *> info = analyzer.getInfo();

  // Determine input format from file extension
  std::string input_format;
  if (ctx.inputFile.find(".y4m") != std::string::npos) {
    input_format = "y4m";
  } else if (ctx.inputFile.find(".yuv") != std::string::npos) {
    input_format = "yuv";
  } else {
    input_format = "unknown";
  }

  // Convert to new format
  motion_search::AnalysisResults results =
      motion_search::DataConverter::convert(
          info, reader->dim().width, reader->dim().height, ctx.gop_size,
          ctx.b_frames, input_format, ctx.inputFile);

  // Get output format and detail level
  std::string format = absl::GetFlag(FLAGS_format);
  std::string detail = absl::GetFlag(FLAGS_detail);
  motion_search::DetailLevel detail_level =
      motion_search::stringToDetailLevel(detail);

  // Open output stream
  std::unique_ptr<std::ostream> out_stream;
  std::ofstream file_stream;

  if (ctx.outputFile == "-") {
    // Use stdout
    out_stream.reset(&std::cout);
  } else {
    // Open file
    file_stream.open(ctx.outputFile);
    if (!file_stream) {
      std::cerr << "Error: Can't open output file " << ctx.outputFile << "\n";
      return 1;
    }
    out_stream.reset(&file_stream);
  }

  // Create and use output writer
  try {
    auto writer = motion_search::createOutputWriter(
        format, detail_level,
        (ctx.outputFile == "-") ? std::cout : file_stream);
    writer->write(results);
  } catch (const std::exception &e) {
    std::cerr << "Error writing output: " << e.what() << "\n";
    return 1;
  }

  const std::chrono::duration<double, std::milli> duration = end - begin;
  std::cerr << "Execution time: " << std::fixed << std::setprecision(2)
            << duration.count() << " msec\n";

  return 0;
}
