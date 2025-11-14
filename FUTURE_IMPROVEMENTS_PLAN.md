# Motion Search Future Improvements Plan

This document outlines a comprehensive roadmap for enhancing the motion-search video complexity analyzer with improved input handling, command-line parsing, output formats, and complexity metrics.

## Executive Summary

The motion-search project has recently completed a major SIMD modernization (Highway migration). Building on this solid foundation, we will enhance the tool's capabilities in four key areas:

1. **Input Flexibility** - Optional FFmpeg integration for diverse video formats
2. **Command-Line Interface** - Modern argument parsing with Abseil
3. **Output Formats** - JSON, XML, and CSV with configurable detail levels
4. **Complexity Metrics** - Learned scoring and improved spatial analysis

---

## Current State

### Strengths
- ✅ Modern C++17 codebase with Google Highway SIMD
- ✅ Comprehensive test suite (34 tests, 4 suites)
- ✅ Clean architecture with separated concerns
- ✅ PMVFAST motion estimation algorithm
- ✅ Active maintenance and documentation

### Limitations
- ⚠️ Input: Only Y4M and raw YUV formats
- ⚠️ CLI: Custom parser (`fb_command_line_parser.h`) lacks modern features
- ⚠️ Output: Single CSV format only
- ⚠️ Metrics: Fixed set, no extensibility
- ⚠️ Complexity: Simple bit estimation without learned weighting

---

## Phase 1: Command-Line Parsing with Abseil

**Priority:** High
**Estimated Effort:** 1-2 weeks
**Dependencies:** None

### Objectives
Replace custom `fb_command_line_parser.h` with Google Abseil Flags library for:
- Long and short option support
- Automatic help generation
- Type-safe argument validation
- Consistent error handling

### Implementation Details

#### 1.1 Add Abseil Dependency
**File:** `CMakeLists.txt`

```cmake
# Add Abseil (similar to Highway integration)
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20240722.0  # LTS branch
)
set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_MakeAvailable(abseil)

# Link against Abseil flags
target_link_libraries(motion_search
  PRIVATE
    motion_search_lib
    absl::flags
    absl::flags_parse
    absl::flags_usage
)
```

#### 1.2 Define Flags
**File:** `main.cpp` (or new `flags.cpp`)

```cpp
// Input options
ABSL_FLAG(std::string, input, "", "Input video file path (required)");
ABSL_FLAG(int32_t, width, 0, "Video width (required for raw YUV)");
ABSL_FLAG(int32_t, height, 0, "Video height (required for raw YUV)");
ABSL_FLAG(std::string, pix_fmt, "yuv420p", "Pixel format (when using FFmpeg)");

// Analysis options
ABSL_FLAG(int32_t, frames, -1, "Number of frames to process (-1 = all)");
ABSL_FLAG(int32_t, gop_size, 150, "GOP size for encoding simulation");
ABSL_FLAG(int32_t, bframes, 0, "Consecutive B-frames");
ABSL_FLAG(int32_t, search_range_h, 32, "Horizontal search range");
ABSL_FLAG(int32_t, search_range_v, 24, "Vertical search range");

// Output options
ABSL_FLAG(std::string, output, "-", "Output file path (- for stdout)");
ABSL_FLAG(std::string, format, "csv", "Output format: csv, json, xml");
ABSL_FLAG(std::string, detail, "gop", "Detail level: frame, gop");
ABSL_FLAG(bool, per_mb, false, "Include per-macroblock data (JSON/XML only)");

// Input format options
ABSL_FLAG(bool, use_ffmpeg, false, "Use FFmpeg for input decoding");
```

#### 1.3 Migrate Parsing Logic
- Remove `fb_command_line_parser.h` include
- Replace manual parsing with `absl::ParseCommandLine()`
- Update help text generation
- Add validation logic for mutually exclusive options

#### 1.4 Validation Rules
```cpp
// Width/height required for YUV, forbidden for Y4M
// FFmpeg requires input file, optional width/height
// Format must be one of: csv, json, xml
// Detail must be one of: frame, gop
```

### Testing
- Update existing integration tests to use new flags
- Add tests for invalid argument combinations
- Verify help text generation
- Test backward compatibility (deprecation warnings for old syntax)

### Migration Path
1. Keep `fb_command_line_parser.h` as deprecated fallback (Phase 1a)
2. Add Abseil alongside existing parser (Phase 1b)
3. Switch default to Abseil, emit warnings for old syntax (Phase 1c)
4. Remove old parser (Phase 1d)

---

## Phase 2: Output Format Enhancements

**Priority:** High
**Estimated Effort:** 2-3 weeks
**Dependencies:** Phase 1 (for CLI flags)

### Objectives
Support multiple output formats with configurable detail levels:
- **JSON:** Per-frame and per-GOP with full metadata
- **XML:** Per-frame and per-GOP with schema validation
- **CSV:** Per-GOP only (lightweight, existing format)

### Implementation Details

#### 2.1 Output Abstraction Layer
**New files:** `OutputWriter.h`, `OutputWriter.cpp`

```cpp
class OutputWriter {
public:
  virtual ~OutputWriter() = default;
  virtual void writeHeader(const VideoMetadata& meta) = 0;
  virtual void writeFrame(const FrameData& frame) = 0;
  virtual void writeGOP(const GOPData& gop) = 0;
  virtual void writeFooter() = 0;
};

class CSVWriter : public OutputWriter { /* ... */ };
class JSONWriter : public OutputWriter { /* ... */ };
class XMLWriter : public OutputWriter { /* ... */ };
```

#### 2.2 Data Structures
**New file:** `OutputData.h`

```cpp
struct VideoMetadata {
  int width, height;
  int total_frames;
  int gop_size, bframes;
  std::string input_format;
  std::chrono::system_clock::time_point analysis_time;
};

struct FrameData {
  int frame_num;
  FrameType type;  // I, P, B

  // Block mode distribution
  int count_intra, count_inter_p, count_inter_b;

  // Complexity metrics
  double spatial_complexity;    // variance-based (current)
  double motion_complexity;     // motion vector magnitude
  double residual_complexity;   // AC energy
  double error_mse;             // reconstruction error
  double unified_complexity;    // learned score (Phase 4)

  // Bit estimation
  int64_t estimated_bits;

  // Motion vector statistics (optional)
  struct MVStats {
    double mean_magnitude;
    double max_magnitude;
    int zero_mv_count;
  } mv_stats;

  // Per-macroblock data (optional, JSON/XML only)
  std::vector<MacroblockData> macroblocks;
};

struct GOPData {
  int gop_num;
  int start_frame, end_frame;
  int64_t total_bits;
  double avg_complexity;
  std::vector<FrameData> frames;  // Only if detail=frame
};
```

#### 2.3 JSON Output Format
**Library:** Use header-only library like `nlohmann/json` or `rapidjson`

```json
{
  "metadata": {
    "width": 1920,
    "height": 1080,
    "frames": 300,
    "gop_size": 150,
    "bframes": 2,
    "input_format": "y4m",
    "analysis_timestamp": "2025-11-14T12:34:56Z",
    "version": "2.0.0"
  },
  "gops": [
    {
      "gop_num": 0,
      "start_frame": 0,
      "end_frame": 149,
      "total_bits": 12456789,
      "avg_complexity": 0.85,
      "frames": [
        {
          "frame_num": 0,
          "type": "I",
          "complexity": {
            "spatial": 0.92,
            "motion": 0.0,
            "residual": 0.88,
            "unified": 0.90
          },
          "block_modes": {
            "intra": 1200,
            "inter_p": 0,
            "inter_b": 0
          },
          "error_mse": 45678,
          "estimated_bits": 123456,
          "mv_stats": {
            "mean_magnitude": 0.0,
            "max_magnitude": 0.0,
            "zero_mv_count": 0
          }
        }
      ]
    }
  ]
}
```

#### 2.4 XML Output Format
**Library:** Use `tinyxml2` or `pugixml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<motion_analysis version="2.0">
  <metadata>
    <video width="1920" height="1080" frames="300"/>
    <encoding gop_size="150" bframes="2"/>
    <input format="y4m"/>
    <timestamp>2025-11-14T12:34:56Z</timestamp>
  </metadata>
  <gops>
    <gop num="0" start="0" end="149" total_bits="12456789" avg_complexity="0.85">
      <frame num="0" type="I">
        <complexity spatial="0.92" motion="0.0" residual="0.88" unified="0.90"/>
        <block_modes intra="1200" inter_p="0" inter_b="0"/>
        <error mse="45678"/>
        <bits estimated="123456"/>
        <mv_stats mean="0.0" max="0.0" zero_count="0"/>
      </frame>
    </gop>
  </gops>
</motion_analysis>
```

#### 2.5 CSV Output Format (Enhanced)
Keep backward compatibility, add optional columns:

```csv
gop,frames,total_bits,avg_complexity,i_frames,p_frames,b_frames
0,150,12456789,0.85,1,148,1
1,150,8234567,0.72,1,148,1
```

**With `--detail=frame` flag:**
```csv
frame,type,spatial,motion,residual,unified,error,bits,intra,inter_p,inter_b
0,I,0.92,0.0,0.88,0.90,45678,123456,1200,0,0
1,P,0.45,0.78,0.52,0.58,23456,67890,300,900,0
```

#### 2.6 Factory Pattern
```cpp
std::unique_ptr<OutputWriter> createOutputWriter(
    const std::string& format,
    const std::string& detail_level,
    std::ostream& out) {
  if (format == "json") return std::make_unique<JSONWriter>(detail_level, out);
  if (format == "xml") return std::make_unique<XMLWriter>(detail_level, out);
  if (format == "csv") return std::make_unique<CSVWriter>(detail_level, out);
  throw std::invalid_argument("Unknown format: " + format);
}
```

### Testing
- Unit tests for each writer with sample data
- Schema validation for JSON (JSON Schema) and XML (XSD)
- Round-trip tests (parse output with external tools)
- Large dataset tests (memory efficiency)
- Backward compatibility tests for CSV format

---

## Phase 3: FFmpeg Input Integration (Optional)

**Priority:** Medium
**Estimated Effort:** 2-3 weeks
**Dependencies:** Phase 1 (for `--use_ffmpeg` flag)

### Objectives
Add optional FFmpeg support for reading diverse video formats:
- MP4, MKV, AVI, WebM containers
- H.264, H.265, VP9, AV1 codecs
- Automatic resolution detection
- Pixel format conversion to YUV420p

### Implementation Details

#### 3.1 Build System Integration
**File:** `CMakeLists.txt`

```cmake
# Optional FFmpeg dependency
option(ENABLE_FFMPEG "Enable FFmpeg input support" OFF)

if(ENABLE_FFMPEG)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
  )

  add_compile_definitions(HAVE_FFMPEG)

  target_include_directories(motion_search_lib PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_libraries(motion_search_lib PRIVATE ${FFMPEG_LIBRARIES})
endif()
```

**Build instructions:**
```bash
cmake -DENABLE_FFMPEG=ON ..
```

#### 3.2 FFmpeg Reader Implementation
**New files:** `FFmpegSequenceReader.h`, `FFmpegSequenceReader.cpp`

```cpp
class FFmpegSequenceReader : public SequenceReader {
private:
  AVFormatContext* format_ctx_ = nullptr;
  AVCodecContext* codec_ctx_ = nullptr;
  SwsContext* sws_ctx_ = nullptr;
  int video_stream_idx_ = -1;
  AVFrame* frame_ = nullptr;
  AVFrame* frame_yuv_ = nullptr;

public:
  FFmpegSequenceReader(const std::string& filepath);
  ~FFmpegSequenceReader() override;

  bool readFrame(yuv_t* yuv_data) override;
  int getWidth() const override { return codec_ctx_->width; }
  int getHeight() const override { return codec_ctx_->height; }

  // Additional metadata
  double getFrameRate() const;
  std::string getCodecName() const;
  int64_t getDuration() const;
};
```

#### 3.3 Factory Pattern for Readers
**Modified file:** `main.cpp`

```cpp
std::unique_ptr<SequenceReader> createSequenceReader(
    const std::string& filepath,
    int width, int height,
    bool use_ffmpeg) {

  if (use_ffmpeg) {
#ifdef HAVE_FFMPEG
    return std::make_unique<FFmpegSequenceReader>(filepath);
#else
    throw std::runtime_error("FFmpeg support not compiled in. "
                             "Rebuild with -DENABLE_FFMPEG=ON");
#endif
  }

  // Auto-detect based on extension
  if (filepath.ends_with(".y4m")) {
    return std::make_unique<Y4MSequenceReader>(filepath);
  } else if (filepath.ends_with(".yuv")) {
    if (width <= 0 || height <= 0) {
      throw std::invalid_argument("Width and height required for YUV files");
    }
    return std::make_unique<YUVSequenceReader>(filepath, width, height);
  }

  // Fallback to FFmpeg if available
#ifdef HAVE_FFMPEG
  std::cerr << "Warning: Unknown extension, attempting FFmpeg decode\n";
  return std::make_unique<FFmpegSequenceReader>(filepath);
#else
  throw std::invalid_argument("Unknown file format and FFmpeg not available");
#endif
}
```

#### 3.4 Error Handling
- Graceful degradation if FFmpeg libraries not found
- Clear error messages for unsupported codecs
- Validation of decoded frame dimensions
- Memory management for AVFrame resources

### Testing
- Unit tests with sample videos (MP4, MKV, WebM)
- Test various codecs (H.264, H.265, VP9)
- Test resolution auto-detection
- Test invalid files and error handling
- Performance comparison vs Y4M (overhead acceptable?)
- CI/CD: Make FFmpeg tests optional (don't fail if lib missing)

### Documentation
- Update README with FFmpeg build instructions
- Document supported formats and codecs
- Add troubleshooting section for codec issues

---

## Phase 4: Unified Complexity Score

**Priority:** Medium
**Estimated Effort:** 3-4 weeks
**Dependencies:** Phase 2 (for output structure)

### Objectives
Design and implement a learned single complexity score that combines:
- Spatial complexity (variance-based, current)
- Motion complexity (MV magnitude)
- Residual complexity (AC energy)
- Error estimation (MSE)

**Initial Implementation:** Simple weighted average with bits-per-pixel normalization
**Future Evolution:** Tuned parameters from regression on real encoding data

### Implementation Details

#### 4.1 Complexity Metric Collection
**Modified file:** `ComplexityAnalyzer.cpp`

```cpp
struct ComplexityMetrics {
  // Existing
  double spatial_variance;
  double motion_magnitude;
  double ac_energy;
  double mse;
  int64_t estimated_bits;

  // New
  double bits_per_pixel;
  double unified_score;

  // Future (Phase 5)
  double dct_energy_weighted;
};

ComplexityMetrics computeFrameComplexity(const FrameData& frame) {
  ComplexityMetrics m;

  int num_pixels = frame.width * frame.height;

  // Normalize to [0, 1] range
  m.spatial_variance = normalizeVariance(frame.intra_variance, num_pixels);
  m.motion_magnitude = normalizeMVMagnitude(frame.avg_mv_magnitude);
  m.ac_energy = normalizeACEnergy(frame.ac_energy, num_pixels);
  m.mse = normalizeMSE(frame.mse, num_pixels);

  m.bits_per_pixel = static_cast<double>(frame.estimated_bits) / num_pixels;

  // Phase 4.2: Initial simple weighted average
  m.unified_score = computeUnifiedScore(m);

  return m;
}
```

#### 4.2 Initial Scoring Function
**Version 1.0:** Bits-per-pixel proxy (baseline)

```cpp
double computeUnifiedScore_v1(const ComplexityMetrics& m) {
  // Simple: use bits-per-pixel as complexity proxy
  // Typical range: 0.05 (low) to 0.5 (high) for compressed video
  return std::clamp(m.bits_per_pixel * 2.0, 0.0, 1.0);
}
```

**Version 2.0:** Weighted linear combination (Phase 4 deliverable)

```cpp
struct ComplexityWeights {
  double w_spatial = 0.25;
  double w_motion = 0.30;
  double w_residual = 0.25;
  double w_error = 0.20;

  // Validate: sum to 1.0
  ComplexityWeights() {
    double sum = w_spatial + w_motion + w_residual + w_error;
    assert(std::abs(sum - 1.0) < 1e-6);
  }
};

double computeUnifiedScore_v2(const ComplexityMetrics& m,
                               const ComplexityWeights& w) {
  double score =
      w.w_spatial * m.spatial_variance +
      w.w_motion * m.motion_magnitude +
      w.w_residual * m.ac_energy +
      w.w_error * m.mse;

  return std::clamp(score, 0.0, 1.0);
}
```

**Version 3.0:** Learned weights (future, post-Phase 4)

```cpp
// TODO: Train on dataset of real encodes
// - Collect ground truth: actual encoding bits from x264/x265
// - Features: spatial, motion, residual, error metrics
// - Target: actual bits-per-pixel or rate-distortion score
// - Method: Linear regression, gradient boosting, or neural net
// - Validate: cross-validation on diverse content (sports, animation, film)

struct LearnedComplexityModel {
  std::vector<double> weights;
  std::vector<double> bias;
  std::string model_version;

  double predict(const ComplexityMetrics& m) const;

  static LearnedComplexityModel loadFromFile(const std::string& path);
};
```

#### 4.3 Normalization Functions
**New file:** `ComplexityNormalization.cpp`

```cpp
// Spatial variance: normalize by pixel range²
double normalizeVariance(double var, int num_pixels) {
  // YUV 8-bit: max variance = (255-0)² = 65025
  constexpr double MAX_VAR = 65025.0;
  return std::sqrt(var / MAX_VAR);  // RMS normalization
}

// Motion magnitude: normalize by frame size
double normalizeMVMagnitude(double avg_mv, int width, int height) {
  double frame_diagonal = std::sqrt(width*width + height*height);
  return std::clamp(avg_mv / (frame_diagonal * 0.1), 0.0, 1.0);
}

// AC energy: normalize by pixel range and count
double normalizeACEnergy(int64_t ac_energy, int num_pixels) {
  // Heuristic: typical range 0-255 per pixel
  double energy_per_pixel = static_cast<double>(ac_energy) / num_pixels;
  return std::clamp(energy_per_pixel / 255.0, 0.0, 1.0);
}

// MSE: normalize by max error (255²)
double normalizeMSE(double mse, int num_pixels) {
  constexpr double MAX_MSE = 255.0 * 255.0;
  return std::sqrt(mse / MAX_MSE);  // RMSE normalization
}
```

#### 4.4 Configuration and Tunability
**New file:** `complexity_weights.json`

```json
{
  "version": "2.0",
  "weights": {
    "spatial": 0.25,
    "motion": 0.30,
    "residual": 0.25,
    "error": 0.20
  },
  "normalization": {
    "variance_method": "rms",
    "mv_magnitude_scale": 0.1,
    "ac_energy_max": 255.0,
    "mse_method": "rmse"
  }
}
```

**CLI flag:**
```cpp
ABSL_FLAG(std::string, complexity_weights, "",
          "Path to complexity weights JSON (default: built-in)");
```

### Testing
- Unit tests for normalization functions
- Regression tests: compare v1 (bpp) vs v2 (weighted) on test videos
- Validate on diverse content types:
  - Low motion (talking head)
  - High motion (sports)
  - High spatial (nature, textures)
  - Low spatial (animation, flat colors)
- Correlation analysis with real encoder bits
- Sensitivity analysis: how do weight changes affect scores?

### Documentation
- Document complexity score formula
- Provide examples of interpretation
- Explain normalization rationale
- Roadmap for learned model integration

---

## Phase 5: Advanced Spatial Complexity (Future)

**Priority:** Low (Placeholder)
**Estimated Effort:** 4-6 weeks
**Dependencies:** Phase 4 (complexity infrastructure)

### Objectives
Replace simple variance-based spatial complexity with **weighted DCT energy approach** for more accurate prediction of intra-coding difficulty.

### Rationale
Current spatial complexity uses variance (second-order moment), which doesn't capture:
- Directional structures (edges, textures)
- Frequency distribution (smooth gradients vs high-frequency detail)
- Intra prediction effectiveness (DC, angular modes)

DCT energy analysis provides:
- Per-frequency band energy (low, mid, high)
- Better correlation with actual DCT-based codecs (H.264, H.265, AV1)
- Weighted sum to emphasize high-frequency detail

### Proposed Approach (TBD)

#### 5.1 DCT Transform per Block
```cpp
// For each 16x16 or 8x8 macroblock
void compute8x8DCT(const uint8_t* block, int stride, double* dct_coeffs);

struct DCTEnergy {
  double dc;          // DC coefficient (mean)
  double low_freq;    // Coefficients 1-7 (first row/col)
  double mid_freq;    // Coefficients 8-31
  double high_freq;   // Coefficients 32-63
  double total;       // Sum of absolute values
};

DCTEnergy analyzeDCTEnergy(const double* dct_coeffs);
```

#### 5.2 Weighted Energy Score
```cpp
double computeWeightedDCTEnergy(const DCTEnergy& e) {
  // Hypothesis: high-frequency content harder to compress
  return 0.1 * e.low_freq +
         0.3 * e.mid_freq +
         0.6 * e.high_freq;
}
```

#### 5.3 Implementation Options

**Option A:** Use existing DCT from codec libraries
- Link against x264/x265 DCT functions
- Pros: Battle-tested, optimized
- Cons: External dependency, licensing

**Option B:** Implement simple 8x8 DCT
- Use separable 1D DCT (rows then columns)
- SIMD optimization with Highway
- Pros: Self-contained, full control
- Cons: Development effort, correctness validation

**Option C:** Approximate with frequency analysis
- Use 2D FFT instead of DCT (similar frequency response)
- Pros: Standard library support (FFTW)
- Cons: Less accurate for 8x8 blocks

#### 5.4 Integration with Unified Score
```cpp
// Add to ComplexityMetrics
struct ComplexityMetrics {
  // ... existing fields ...

  // Phase 5
  double dct_energy_low;
  double dct_energy_mid;
  double dct_energy_high;
  double dct_energy_weighted;
};

// Update unified score to use DCT energy instead of variance
double computeUnifiedScore_v3(const ComplexityMetrics& m,
                               const ComplexityWeights& w) {
  double score =
      w.w_spatial * m.dct_energy_weighted +  // Replaces variance
      w.w_motion * m.motion_magnitude +
      w.w_residual * m.ac_energy +
      w.w_error * m.mse;

  return std::clamp(score, 0.0, 1.0);
}
```

### Research Tasks (Pre-Implementation)
1. Literature review: DCT-based complexity metrics in academia
2. Experiment: Correlation study (variance vs DCT energy vs actual bits)
3. Prototype: Python/MATLAB proof-of-concept with sample videos
4. Benchmark: Performance impact of 8x8 DCT per macroblock
5. Validation: Compare against x264's complexity estimation (if accessible)

### Open Questions
- Which frequency bands correlate best with compression difficulty?
- Should we account for intra prediction modes? (directionality)
- How does this interact with screen content (text, graphics)?
- Performance overhead: acceptable latency increase?

### Status: **PLACEHOLDER - DESIGN PHASE**
This phase requires further research and prototyping before implementation planning.

---

## Phase 6: Additional Enhancements (Optional)

### 6.1 Parallel Processing
- Multi-threaded frame analysis (thread pool)
- Parallel GOP processing
- SIMD already optimized (Highway)

### 6.2 Progress Reporting
- Real-time progress bar (e.g., indicators library)
- ETA estimation based on processed frames
- Quiet mode for scripting (`--quiet` flag)

### 6.3 Configuration Files
- YAML/TOML config for default settings
- Per-project analysis profiles
- Override via CLI flags

### 6.4 Advanced Metrics
- Scene change detection (histogram difference)
- Film grain estimation (noise analysis)
- Color complexity (chroma variance)
- Per-region complexity (spatial heatmap)

### 6.5 Export Formats
- SQLite database for large-scale analysis
- Protobuf for inter-tool communication
- Integration with video analysis pipelines

---

## Implementation Roadmap

### Timeline (Estimated)

| Phase | Duration | Deliverables |
|-------|----------|--------------|
| Phase 1: Abseil CLI | 1-2 weeks | Modern command-line parsing |
| Phase 2: Output Formats | 2-3 weeks | JSON, XML, CSV writers |
| Phase 3: FFmpeg Input | 2-3 weeks | Optional FFmpeg integration |
| Phase 4: Unified Score | 3-4 weeks | Learned complexity metric |
| Phase 5: DCT Energy | 4-6 weeks | Advanced spatial analysis (TBD) |

**Total:** ~12-18 weeks for Phases 1-4
**Phase 5:** Requires research phase first

### Dependency Graph

```
Phase 1 (CLI)
    ↓
    ├─→ Phase 2 (Output) ──→ Phase 4 (Unified Score) ──→ Phase 5 (DCT)
    └─→ Phase 3 (FFmpeg)
```

Phases 2 and 3 can be developed in parallel after Phase 1.

### Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Abseil breaking changes | Pin to LTS version (20240722.0) |
| FFmpeg API instability | Abstract behind SequenceReader interface |
| Performance regression | Benchmark suite before/after each phase |
| Output format bloat | Lazy evaluation, streaming writers |
| Complexity score inaccuracy | Validate against real encoder data |

---

## Success Criteria

### Phase 1 (CLI)
- ✅ All existing functionality accessible via new flags
- ✅ Automatic help generation works
- ✅ Backward compatibility or clear migration path
- ✅ All tests pass with new CLI

### Phase 2 (Output)
- ✅ JSON/XML output validates against schema
- ✅ CSV format maintains backward compatibility
- ✅ Per-frame and per-GOP modes both work
- ✅ Output parseable by standard tools (jq, xmllint)

### Phase 3 (FFmpeg)
- ✅ MP4, MKV, WebM inputs work correctly
- ✅ Auto-detection of resolution works
- ✅ Graceful degradation without FFmpeg
- ✅ Performance overhead < 10% vs Y4M

### Phase 4 (Unified Score)
- ✅ Score correlates with encoding difficulty
- ✅ Normalization produces consistent [0,1] range
- ✅ Weights are configurable
- ✅ Documentation explains interpretation

### Phase 5 (DCT)
- ✅ DCT energy correlates better than variance
- ✅ Performance overhead acceptable
- ✅ Validated against reference encoder

---

## Maintenance and Long-Term Vision

### Code Quality
- Maintain test coverage > 80%
- Document all new APIs
- Follow existing code style (clang-format)
- Regular dependency updates (Dependabot)

### Community
- Provide migration guides for each phase
- Accept community feedback on complexity weights
- Publish benchmark results and validation data
- Open-source trained models (Phase 4)

### Future Directions
- Machine learning model for complexity (Phase 4+)
- GPU acceleration for SIMD kernels
- Real-time video analysis (webcam input)
- Integration with encoding pipelines (FFmpeg filters)
- Web-based visualization dashboard

---

## Conclusion

This comprehensive plan transforms motion-search from a specialized Y4M analysis tool into a flexible, production-ready video complexity analyzer. The phased approach ensures:

1. **Incremental value delivery** - Each phase provides standalone benefits
2. **Risk management** - Dependencies isolated, failures contained
3. **Community involvement** - Clear roadmap enables external contributions
4. **Future-proofing** - Extensible architecture supports long-term evolution

The recent Highway migration demonstrates the project's commitment to modernization. These improvements continue that trajectory, positioning motion-search as a best-in-class open-source video analysis tool.

---

**Document Version:** 1.0
**Last Updated:** 2025-11-14
**Status:** Planning Phase
**Next Steps:** Review with stakeholders, prioritize phases, begin Phase 1 implementation
