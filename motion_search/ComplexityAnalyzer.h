/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "common.h"

#include "ComplexityNormalization.h"
#include "IVideoSequenceReader.h"
#include "MotionVectorField.h"
#include "memory.h"

#include <vector>

using std::vector;

typedef struct {
  int picNum;
  int picType;
  int count_I;
  int count_P;
  int count_B;
  int bits;
  int error;

  // Phase 4: Enhanced complexity metrics
  double spatial_variance; // Raw spatial variance
  double motion_magnitude; // Average motion vector magnitude
  int64_t ac_energy;       // AC energy (residual complexity)
  double bits_per_pixel;   // Bits per pixel ratio
  double unified_score_v1; // Unified score v1.0 (bpp-based)
  double unified_score_v2; // Unified score v2.0 (weighted)
  double norm_spatial;     // Normalized spatial complexity [0,1]
  double norm_motion;      // Normalized motion complexity [0,1]
  double norm_residual;    // Normalized residual complexity [0,1]
  double norm_error;       // Normalized error [0,1]
} complexity_info_t;

class ComplexityAnalyzer {
public:
  ComplexityAnalyzer(IVideoSequenceReader *reader, int gop_size, int num_frames,
                     int b_frames);

  ~ComplexityAnalyzer(void);

  void analyze(void);

  vector<complexity_info_t *> getInfo() { return m_info; }

  // Phase 4: Set complexity weights for unified scoring
  void setComplexityWeights(const complexity::ComplexityWeights &weights) {
    m_weights = weights;
  }

private:
  DIM m_dim;
  int m_stride;
  int m_padded_height;
  int m_num_frames;

  int m_GOP_size;
  int m_subGOP_size;

  int m_GOP_error;
  int m_GOP_bits;
  int m_GOP_count;

  vector<YUVFrame *> pics;

  MotionVectorField *m_pPmv;
  MotionVectorField *m_pB1mv;
  MotionVectorField *m_pB2mv;

  memory::aligned_unique_ptr<int> m_mses;
  memory::aligned_unique_ptr<unsigned char> m_MB_modes;

  IVideoSequenceReader *m_pReader;

  vector<complexity_info_t *> m_info;
  complexity_info_t *m_pReorderedInfo;

  // Phase 4: Complexity weights for unified scoring
  complexity::ComplexityWeights m_weights;

  void reset_gop_start(void);

  void add_info(int num, char p, int err, int count_I, int count_P, int count_B,
                int bits);

  // Phase 4: Enhanced add_info with complexity metrics
  void add_info_enhanced(int num, char p, int err, int count_I, int count_P,
                         int count_B, int bits,
                         const complexity::ComplexityMetrics &metrics);

  void process_i_picture(YUVFrame *pict);

  void process_p_picture(YUVFrame *pict, YUVFrame *ref);

  void process_b_picture(YUVFrame *pict, YUVFrame *fwdref, YUVFrame *backref);

  // Phase 4: Helper methods for computing enhanced metrics
  double computeSpatialVariance(YUVFrame *pict);
  double computeMotionMagnitude(MotionVectorField *mvField);
  int64_t computeACEnergy(int *mses, int num_blocks);

  ComplexityAnalyzer(ComplexityAnalyzer &) = delete;

  ComplexityAnalyzer &operator=(ComplexityAnalyzer &) = delete;
};
