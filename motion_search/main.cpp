
/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "ComplexityAnalyzer.h"
#include "Y4MSequenceReader.h"
#include "YUVSequenceReader.h"

#include <algorithm>
#include <chrono>
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
};

std::unique_ptr<IVideoSequenceReader> getReader(const std::string &filename,
                                                const DIM dim) {
  std::unique_ptr<IVideoSequenceReader> reader;

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

  // Note: For Phase 1, only CSV is implemented
  if (format != "csv") {
    std::cerr << "Warning: Only CSV format is currently implemented\n";
    std::cerr << "JSON and XML formats will be added in Phase 2\n";
    std::cerr << "Falling back to CSV format\n";
  }
}

/**
 * @brief Print the complexity information to the output file in CSV format
 *
 * @param pOut The output file
 * @param i The complexity information
 */
void print_compl_inf(FILE *const pOut, complexity_info_t *i) {
  fprintf(pOut, "%d,%c,%d,%d,%d,%d,%d\n", i->picNum, i->picType, i->count_I,
          i->count_P, i->count_B, i->error, i->bits);
}

} // namespace

int main(int argc, char *argv[]) {
  // Set up usage message
  absl::SetProgramUsageMessage(
      "Motion Search Video Complexity Analyzer\n\n"
      "Analyzes video complexity using motion estimation and spatial "
      "analysis.\n"
      "Supports Y4M and raw YUV input formats.\n\n"
      "Usage:\n"
      "  motion_search --input=<file> --output=<file> [options]\n"
      "  motion_search <input_file> <output_file> [options]  (legacy syntax)\n"
      "\n"
      "Examples:\n"
      "  motion_search --input=video.y4m --output=results.csv\n"
      "  motion_search --input=video.yuv --width=1920 --height=1080 "
      "--output=results.csv\n"
      "  motion_search video.y4m results.csv -g=60 -b=2  (legacy syntax)\n"
      "\n"
      "Required flags:\n"
      "  --input=<file>   Input video file (.y4m or .yuv)\n"
      "  --output=<file>  Output CSV file (use '-' for stdout)\n"
      "\n"
      "Optional flags:\n"
      "  --width=<n>      Video width (required for .yuv files)\n"
      "  --height=<n>     Video height (required for .yuv files)\n"
      "  --frames=<n>     Number of frames to process (0 = all, default: 0)\n"
      "  --gop_size=<n>   GOP size for simulation (default: 150)\n"
      "  --bframes=<n>    Number of consecutive B-frames (default: 0)\n"
      "  --format=<fmt>   Output format: csv (default), json, xml (Phase 2)\n"
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

  auto reader = getReader(ctx.inputFile, {ctx.width, ctx.height});
  if (reader == nullptr) {
    std::cerr << "Error: Unsupported input format for " << ctx.inputFile
              << "\n";
    std::cerr << "Supported formats: .y4m, .yuv\n";
    std::cerr << "For .yuv files, you must specify --width and --height\n";
    return 1;
  }

  ComplexityAnalyzer analyzer(reader.get(), ctx.gop_size, ctx.num_frames,
                              ctx.b_frames);

  const auto begin = std::chrono::high_resolution_clock::now();
  analyzer.analyze();
  const auto end = std::chrono::high_resolution_clock::now();

  // Open output file
  FILE *out_fp = nullptr;
  if (ctx.outputFile == "-") {
    out_fp = stdout;
  } else {
    out_fp = fopen(ctx.outputFile.c_str(), "w");
    if (!out_fp) {
      std::cerr << "Error: Can't open output file " << ctx.outputFile << "\n";
      return 1;
    }
  }
  std::unique_ptr<FILE, file_closer> out(out_fp);

  std::cerr << "Input file: '" << ctx.inputFile << "'\n";
  std::cerr << "width: " << reader->dim().width << "\n";
  std::cerr << "height: " << reader->dim().height << "\n";

  // write CSV header
  fprintf(out.get(), "picNum,picType,count_I,count_P,count_B,error,bits\n");

  // write each frame data individually
  vector<complexity_info_t *> info = analyzer.getInfo();
  for_each(info.begin(), info.end(),
           [&](complexity_info_t *i) { print_compl_inf(out.get(), i); });

  const std::chrono::duration<double, std::milli> duration = end - begin;
  std::cerr << "Execution time: " << std::fixed << std::setprecision(2)
            << duration.count() << " msec\n";

  return 0;
}
