/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "ComplexityAnalyzer.h"
#include "ComplexityNormalization.h"
#include "EOFException.h"

#include "moments.h"

#include <cmath>

ComplexityAnalyzer::ComplexityAnalyzer(IVideoSequenceReader *reader,
                                       int gop_size, int num_frames,
                                       int b_frames)
    : m_dim(reader->dim()),
      m_stride(reader->dim().width + 2 * HORIZONTAL_PADDING),
      m_padded_height(reader->dim().height + 2 * VERTICAL_PADDING),
      m_num_frames(num_frames), m_GOP_size(gop_size),
      m_subGOP_size(b_frames + 1), m_pReader(reader), m_pReorderedInfo(NULL) {
  m_GOP_error = 0;
  m_GOP_bits = 0;
  m_GOP_count = 0;

  for (int i = 0; i <= m_subGOP_size; i++)
    pics.push_back(new YUVFrame(m_pReader));

  m_pPmv = new MotionVectorField(m_dim, m_stride, m_padded_height, MB_WIDTH);
  m_pB1mv = new MotionVectorField(m_dim, m_stride, m_padded_height, MB_WIDTH);
  m_pB2mv = new MotionVectorField(m_dim, m_stride, m_padded_height, MB_WIDTH);

  int stride_MB = m_dim.width / MB_WIDTH + 2;
  int padded_height_MB = (m_dim.height + MB_WIDTH - 1) / MB_WIDTH + 2;

  const size_t numItems = (size_t)(stride_MB) * (padded_height_MB);
  m_mses = memory::AlignedAlloc<int>(numItems);
  if (m_mses == NULL) {
    fprintf(stderr, "Not enough memory (%zu bytes) for %s\n",
            numItems * sizeof(int), "m_mses");
    exit(-1);
  }
  m_MB_modes = memory::AlignedAlloc<unsigned char>(numItems);
  if (m_MB_modes == NULL) {
    fprintf(stderr, "Not enough memory (%zu bytes) for %s\n",
            numItems * sizeof(unsigned char), "m_MB_modes");
    exit(-1);
  }
}

ComplexityAnalyzer::~ComplexityAnalyzer(void) {
  pics.clear();

  delete m_pPmv;
  delete m_pB1mv;
  delete m_pB2mv;
}

void ComplexityAnalyzer::reset_gop_start(void) {
  m_pPmv->reset();
  m_pB1mv->reset();
  m_pB2mv->reset();
}

void ComplexityAnalyzer::add_info(int num, char p, int err, int count_I,
                                  int count_P, int count_B, int bits) {
  complexity_info_t *i = new complexity_info_t;

  // Convert all numbering to 0..N-1
  i->picNum = num - 1;
  i->picType = p;
  i->error = err;
  i->count_I = count_I;
  i->count_P = count_P;
  i->count_B = count_B;
  i->bits = bits;

  // Phase 4: Initialize new fields to zero for backward compatibility
  i->spatial_variance = 0.0;
  i->motion_magnitude = 0.0;
  i->ac_energy = 0;
  i->bits_per_pixel = 0.0;
  i->unified_score_v1 = 0.0;
  i->unified_score_v2 = 0.0;
  i->norm_spatial = 0.0;
  i->norm_motion = 0.0;
  i->norm_residual = 0.0;
  i->norm_error = 0.0;

  if (p == 'I' || p == 'P') {
    if (m_pReorderedInfo != NULL)
      m_info.push_back(m_pReorderedInfo);
    m_pReorderedInfo = i;
  } else {
    m_info.push_back(i);
  }
}

void ComplexityAnalyzer::add_info_enhanced(
    int num, char p, int err, int count_I, int count_P, int count_B, int bits,
    const complexity::ComplexityMetrics &metrics) {
  complexity_info_t *i = new complexity_info_t;

  // Convert all numbering to 0..N-1
  i->picNum = num - 1;
  i->picType = p;
  i->error = err;
  i->count_I = count_I;
  i->count_P = count_P;
  i->count_B = count_B;
  i->bits = bits;

  // Phase 4: Enhanced complexity metrics
  i->spatial_variance = metrics.spatial_variance;
  i->motion_magnitude = metrics.motion_magnitude;
  i->ac_energy = metrics.ac_energy;
  i->bits_per_pixel = metrics.bits_per_pixel;
  i->unified_score_v1 = metrics.unified_score_v1;
  i->unified_score_v2 = metrics.unified_score_v2;
  i->norm_spatial = metrics.norm_spatial;
  i->norm_motion = metrics.norm_motion;
  i->norm_residual = metrics.norm_residual;
  i->norm_error = metrics.norm_error;

  if (p == 'I' || p == 'P') {
    if (m_pReorderedInfo != NULL)
      m_info.push_back(m_pReorderedInfo);
    m_pReorderedInfo = i;
  } else {
    m_info.push_back(i);
  }
}

void ComplexityAnalyzer::process_i_picture(YUVFrame *pict) {
  reset_gop_start();
  int error = m_pPmv->predictSpatial(pict, &m_mses.get()[m_pPmv->firstMB()],
                                     &m_MB_modes.get()[m_pPmv->firstMB()]);
  int bits = m_pPmv->bits();

  // We are weighting I-frames by 10% more bits (282/256), since the QP needs to
  // be the lowest among I/P/B
  bits = (I_FRAME_BIT_WEIGHT * bits + 128) >> 8;
  m_GOP_bits += bits;
  m_GOP_error += error;

  // Phase 4: Compute enhanced metrics
  complexity::ComplexityMetrics metrics;
  metrics.spatial_variance = computeSpatialVariance(pict);
  metrics.motion_magnitude = 0.0; // I-frames have no motion vectors
  int num_blocks = (m_dim.width / MB_WIDTH) * (m_dim.height / MB_WIDTH);
  metrics.ac_energy =
      computeACEnergy(&m_mses.get()[m_pPmv->firstMB()], num_blocks);
  metrics.mse = static_cast<double>(error);
  metrics.estimated_bits = bits;

  // Normalize and compute scores
  complexity::normalizeMetrics(metrics, m_dim.width, m_dim.height);
  metrics.unified_score_v1 = complexity::computeUnifiedScore_v1(metrics);
  metrics.unified_score_v2 =
      complexity::computeUnifiedScore_v2(metrics, m_weights);

  add_info_enhanced(m_pReader->count(), 'I', error, m_pPmv->count_I(), 0, 0,
                    bits, metrics);

  // for debugging
  // fprintf(stderr, "Frame %6d (I), I:%6d, P:%6d, B:%6d, MSE = %9d, bits =
  // %7d\n",pict->pos()+1,m_pPmv->count_I(),0,0,error,bits);
  pict->boundaryExtend();
}

void ComplexityAnalyzer::process_p_picture(YUVFrame *pict, YUVFrame *ref) {
  int error =
      m_pPmv->predictTemporal(pict, ref, &m_mses.get()[m_pPmv->firstMB()],
                              &m_MB_modes.get()[m_pPmv->firstMB()]);
  int bits = m_pPmv->bits();

  // We are weighting P-frames by 5% more bits (269/256), since the QP needs to
  // be lower than B (but higher than I)
  bits = (P_FRAME_BIT_WEIGHT * bits + 128) >> 8;
  m_GOP_bits += bits;
  m_GOP_error += error;

  // Phase 4: Compute enhanced metrics
  complexity::ComplexityMetrics metrics;
  metrics.spatial_variance = computeSpatialVariance(pict);
  metrics.motion_magnitude = computeMotionMagnitude(m_pPmv);
  int num_blocks = (m_dim.width / MB_WIDTH) * (m_dim.height / MB_WIDTH);
  metrics.ac_energy =
      computeACEnergy(&m_mses.get()[m_pPmv->firstMB()], num_blocks);
  metrics.mse = static_cast<double>(error);
  metrics.estimated_bits = bits;

  // Normalize and compute scores
  complexity::normalizeMetrics(metrics, m_dim.width, m_dim.height);
  metrics.unified_score_v1 = complexity::computeUnifiedScore_v1(metrics);
  metrics.unified_score_v2 =
      complexity::computeUnifiedScore_v2(metrics, m_weights);

  add_info_enhanced(m_pReader->count(), 'P', error, m_pPmv->count_I(),
                    m_pPmv->count_P(), 0, bits, metrics);

  // for debugging
  // fprintf(stderr, "Frame %6d (P), I:%6d, P:%6d, B:%6d, MSE = %9d, bits =
  // %7d\n",pict->pos()+1,m_pPmv->count_I(),m_pPmv->count_P(),0,error,bits);
  pict->boundaryExtend();
}

void ComplexityAnalyzer::process_b_picture(YUVFrame *pict, YUVFrame *fwdref,
                                           YUVFrame *backref) {
  int error = m_pPmv->predictBidirectional(
      pict, fwdref, backref, m_pB1mv, m_pB2mv, &m_mses.get()[m_pPmv->firstMB()],
      &m_MB_modes.get()[m_pPmv->firstMB()]);
  int bits = m_pPmv->bits();

  // We are weighting B-frames by 0% more bits (256/256), since QP needs to be
  // highest among I/P/B
  bits = (B_FRAME_BIT_WEIGHT * bits + 128) >> 8;
  m_GOP_bits += bits;
  m_GOP_error += error;

  // Phase 4: Compute enhanced metrics
  complexity::ComplexityMetrics metrics;
  metrics.spatial_variance = computeSpatialVariance(pict);
  metrics.motion_magnitude = computeMotionMagnitude(m_pPmv);
  int num_blocks = (m_dim.width / MB_WIDTH) * (m_dim.height / MB_WIDTH);
  metrics.ac_energy =
      computeACEnergy(&m_mses.get()[m_pPmv->firstMB()], num_blocks);
  metrics.mse = static_cast<double>(error);
  metrics.estimated_bits = bits;

  // Normalize and compute scores
  complexity::normalizeMetrics(metrics, m_dim.width, m_dim.height);
  metrics.unified_score_v1 = complexity::computeUnifiedScore_v1(metrics);
  metrics.unified_score_v2 =
      complexity::computeUnifiedScore_v2(metrics, m_weights);

  add_info_enhanced(m_pReader->count() - (backref->pos() - pict->pos()), 'B',
                    error, m_pPmv->count_I(), m_pPmv->count_P(),
                    m_pPmv->count_B(), bits, metrics);

  // for debugging
  // fprintf(stderr, "Frame %6d (B), I:%6d, P:%6d, B:%6d, MSE = %9d, bits =
  // %7d\n",pict->pos()+1,m_pPmv->count_I(),m_pPmv->count_P(),m_pPmv->count_B(),error,bits);
}

void ComplexityAnalyzer::analyze() {
  int td = 0;
  int td_ref;

  try {
    while (m_num_frames > 0 ? m_pReader->count() < m_num_frames
                            : !m_pReader->eof()) {
      fprintf(stderr, "Picture count: %d\r", m_pReader->count() - 1);

      if ((m_pReader->count() % m_GOP_size) == 0) {
        if (m_pReader->count()) {
          fprintf(stderr, "GOP: %d, GOP-bits: %d\n", m_GOP_count, m_GOP_bits);
          m_GOP_count++;
        }
        m_GOP_error = 0;
        m_GOP_bits = 0;

        td = 0;
        pics[0]->readNextFrame();
        process_i_picture(pics[0]);
      } else {
        pics[0]->swapFrame(pics[(size_t)m_subGOP_size]);
      }

      for (td_ref = td; td < (m_GOP_size - 1) && (td - td_ref) < m_subGOP_size;
           td++) {
        pics[(size_t)(td + 1 - td_ref)]->readNextFrame();
      }

      process_p_picture(/* target    */ pics[(size_t)(td - td_ref)],
                        /* reference */ pics[0]);

      for (int j = 1; j < td - td_ref; j++) {
        process_b_picture(/* target  */ pics[(size_t)j],
                          /* forward */ pics[0],
                          /* reverse */ pics[(size_t)(td - td_ref)]);
      }
    }
  } catch (EOFException &e) {
    fprintf(stderr, "\n%s\n", e.what());
  }

  if (m_pReorderedInfo != NULL)
    m_info.push_back(m_pReorderedInfo);

  fprintf(stderr, "Processed frames: %d\n", m_pReader->count());
}

// Phase 4: Helper methods for computing enhanced metrics

double ComplexityAnalyzer::computeSpatialVariance(YUVFrame *pict) {
  // Compute average variance across all blocks in the frame
  int num_blocks_x = m_dim.width / MB_WIDTH;
  int num_blocks_y = m_dim.height / MB_WIDTH;
  int num_blocks = num_blocks_x * num_blocks_y;

  if (num_blocks == 0) {
    return 0.0;
  }

  int64_t total_variance = 0;
  unsigned char *y_data = pict->y();

  for (int by = 0; by < num_blocks_y; by++) {
    for (int bx = 0; bx < num_blocks_x; bx++) {
      unsigned char *block =
          y_data + by * MB_WIDTH * pict->stride() + bx * MB_WIDTH;
      int var = fast_variance16(block, pict->stride(), MB_WIDTH, MB_WIDTH);
      total_variance += var;
    }
  }

  return static_cast<double>(total_variance) / num_blocks;
}

double ComplexityAnalyzer::computeMotionMagnitude(MotionVectorField *mvField) {
  // Compute average motion vector magnitude across all blocks
  int num_blocks_x = m_dim.width / MB_WIDTH;
  int num_blocks_y = m_dim.height / MB_WIDTH;
  int num_blocks = num_blocks_x * num_blocks_y;

  if (num_blocks == 0) {
    return 0.0;
  }

  MV *mvs = mvField->MVs();
  int stride_MB = m_dim.width / MB_WIDTH + 2;

  double total_magnitude = 0.0;
  for (int by = 0; by < num_blocks_y; by++) {
    for (int bx = 0; bx < num_blocks_x; bx++) {
      int idx = (by + 1) * stride_MB + (bx + 1);
      MV mv = mvs[idx];
      double magnitude =
          std::sqrt(static_cast<double>(mv.x * mv.x + mv.y * mv.y));
      total_magnitude += magnitude;
    }
  }

  return total_magnitude / num_blocks;
}

int64_t ComplexityAnalyzer::computeACEnergy(int *mses, int num_blocks) {
  // AC energy is the sum of MSEs (which represent residual energy)
  // This is already computed and stored in mses array
  if (num_blocks == 0) {
    return 0;
  }

  int64_t total_energy = 0;
  for (int i = 0; i < num_blocks; i++) {
    total_energy += mses[i];
  }

  return total_energy;
}
