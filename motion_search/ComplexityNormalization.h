/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cmath>
#include <cstdint>

/**
 * @brief Complexity normalization utilities for Phase 4 unified scoring
 *
 * This module provides functions to normalize various complexity metrics
 * to a consistent [0, 1] range, enabling meaningful combination into a
 * unified complexity score.
 */
namespace complexity {

/**
 * @brief Normalize spatial variance to [0, 1] range
 *
 * Uses RMS normalization based on the maximum possible variance
 * for 8-bit YUV video (255² = 65025).
 *
 * @param variance Raw variance value
 * @param num_pixels Number of pixels in the frame
 * @return Normalized spatial complexity score [0, 1]
 */
double normalizeVariance(double variance, int num_pixels);

/**
 * @brief Normalize motion vector magnitude to [0, 1] range
 *
 * Normalizes based on a fraction of the frame diagonal.
 * Typical motion is much smaller than frame size, so we use
 * 10% of diagonal as reference.
 *
 * @param avg_mv_magnitude Average MV magnitude across all blocks
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @return Normalized motion complexity score [0, 1]
 */
double normalizeMVMagnitude(double avg_mv_magnitude, int width, int height);

/**
 * @brief Normalize AC energy to [0, 1] range
 *
 * AC energy represents high-frequency content after DC removal.
 * Normalized by typical per-pixel energy range.
 *
 * @param ac_energy Total AC energy across all blocks
 * @param num_pixels Number of pixels in the frame
 * @return Normalized residual complexity score [0, 1]
 */
double normalizeACEnergy(int64_t ac_energy, int num_pixels);

/**
 * @brief Normalize MSE (Mean Squared Error) to [0, 1] range
 *
 * Uses RMSE normalization based on maximum possible error
 * for 8-bit video (255²).
 *
 * @param mse Mean squared error value
 * @return Normalized error score [0, 1]
 */
double normalizeMSE(double mse);

/**
 * @brief Compute bits per pixel for a frame
 *
 * Simple ratio of estimated bits to total pixels.
 * Typical range: 0.05 (low complexity) to 0.5+ (high complexity)
 *
 * @param estimated_bits Estimated encoding bits for the frame
 * @param num_pixels Number of pixels in the frame
 * @return Bits per pixel value
 */
double computeBitsPerPixel(int64_t estimated_bits, int num_pixels);

/**
 * @brief Configuration weights for unified complexity scoring
 *
 * Weights must sum to 1.0 for meaningful interpretation.
 * Default values are based on empirical analysis (can be tuned).
 */
struct ComplexityWeights {
  double w_spatial = 0.25;  // Spatial variance/texture weight
  double w_motion = 0.30;   // Motion vector magnitude weight
  double w_residual = 0.25; // Residual/AC energy weight
  double w_error = 0.20;    // Reconstruction error weight

  /**
   * @brief Validate that weights sum to 1.0 (within epsilon)
   * @return true if weights are valid, false otherwise
   */
  bool isValid() const;

  /**
   * @brief Get sum of all weights
   * @return Sum of w_spatial + w_motion + w_residual + w_error
   */
  double sum() const;
};

/**
 * @brief Normalized complexity metrics for a single frame
 *
 * All metrics are normalized to [0, 1] range for consistent
 * combination into unified score.
 */
struct ComplexityMetrics {
  // Raw metrics
  double spatial_variance = 0.0; // Raw variance value
  double motion_magnitude = 0.0; // Average MV magnitude
  int64_t ac_energy = 0;         // AC energy (high-freq content)
  double mse = 0.0;              // Mean squared error
  int64_t estimated_bits = 0;    // Estimated encoding bits

  // Normalized metrics [0, 1]
  double norm_spatial = 0.0;  // Normalized spatial complexity
  double norm_motion = 0.0;   // Normalized motion complexity
  double norm_residual = 0.0; // Normalized residual complexity
  double norm_error = 0.0;    // Normalized reconstruction error

  // Derived metrics
  double bits_per_pixel = 0.0; // Bits per pixel ratio

  // Unified scores (Phase 4 deliverable)
  double unified_score_v1 = 0.0; // v1.0: bits-per-pixel based [0, 1]
  double unified_score_v2 = 0.0; // v2.0: weighted combination [0, 1]
};

/**
 * @brief Compute unified complexity score v1.0 (bits-per-pixel based)
 *
 * Simple baseline: uses bits-per-pixel as a proxy for complexity.
 * Typical compressed video: 0.05-0.5 bpp, scaled to [0, 1].
 *
 * @param metrics Complexity metrics with bits_per_pixel computed
 * @return Unified complexity score [0, 1]
 */
double computeUnifiedScore_v1(const ComplexityMetrics &metrics);

/**
 * @brief Compute unified complexity score v2.0 (weighted combination)
 *
 * Combines normalized spatial, motion, residual, and error metrics
 * using configurable weights. This is the Phase 4 deliverable.
 *
 * Formula:
 *   score = w_spatial * norm_spatial +
 *           w_motion * norm_motion +
 *           w_residual * norm_residual +
 *           w_error * norm_error
 *
 * @param metrics Complexity metrics with normalized values
 * @param weights Weighting configuration
 * @return Unified complexity score [0, 1]
 */
double computeUnifiedScore_v2(const ComplexityMetrics &metrics,
                              const ComplexityWeights &weights);

/**
 * @brief Normalize all metrics in ComplexityMetrics structure
 *
 * Convenience function that applies all normalization functions
 * and populates the norm_* fields in metrics.
 *
 * @param metrics Metrics structure with raw values filled
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 */
void normalizeMetrics(ComplexityMetrics &metrics, int width, int height);

} // namespace complexity
