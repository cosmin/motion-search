/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "ComplexityNormalization.h"

#include <algorithm>
#include <cmath>

namespace complexity {

double normalizeVariance(double variance, int num_pixels) {
  // YUV 8-bit: max variance = (255-0)² = 65025
  constexpr double MAX_VAR = 65025.0;

  // Use RMS (root mean square) normalization
  // This gives better scale interpretation than linear
  double normalized = std::sqrt(variance / MAX_VAR);

  return std::clamp(normalized, 0.0, 1.0);
}

double normalizeMVMagnitude(double avg_mv_magnitude, int width, int height) {
  // Compute frame diagonal
  double frame_diagonal =
      std::sqrt(static_cast<double>(width * width + height * height));

  // Use 10% of diagonal as reference (empirically chosen)
  // Most motion is much smaller than full frame displacement
  double reference_magnitude = frame_diagonal * 0.1;

  double normalized = avg_mv_magnitude / reference_magnitude;

  return std::clamp(normalized, 0.0, 1.0);
}

double normalizeACEnergy(int64_t ac_energy, int num_pixels) {
  // Heuristic: typical AC energy range is 0-255 per pixel
  // (based on 8-bit residuals after prediction)
  double energy_per_pixel = static_cast<double>(ac_energy) / num_pixels;

  double normalized = energy_per_pixel / 255.0;

  return std::clamp(normalized, 0.0, 1.0);
}

double normalizeMSE(double mse) {
  // MSE range: 0 (perfect match) to 255² (max error for 8-bit)
  constexpr double MAX_MSE = 255.0 * 255.0;

  // Use RMSE (root mean square error) normalization
  double normalized = std::sqrt(mse / MAX_MSE);

  return std::clamp(normalized, 0.0, 1.0);
}

double computeBitsPerPixel(int64_t estimated_bits, int num_pixels) {
  if (num_pixels <= 0) {
    return 0.0;
  }
  return static_cast<double>(estimated_bits) / num_pixels;
}

bool ComplexityWeights::isValid() const {
  constexpr double EPSILON = 1e-6;
  double weight_sum = sum();
  return std::abs(weight_sum - 1.0) < EPSILON;
}

double ComplexityWeights::sum() const {
  return w_spatial + w_motion + w_residual + w_error;
}

double computeUnifiedScore_v1(const ComplexityMetrics &metrics) {
  // Version 1.0: Simple bits-per-pixel based score
  // Typical compressed video ranges:
  //   - Low complexity: 0.05 bpp (talking head, screen content)
  //   - Medium complexity: 0.15 bpp (normal video)
  //   - High complexity: 0.5+ bpp (high motion, high detail)
  //
  // We scale by 2.0 to map 0.5 bpp -> 1.0 (full complexity)
  double score = metrics.bits_per_pixel * 2.0;

  return std::clamp(score, 0.0, 1.0);
}

double computeUnifiedScore_v2(const ComplexityMetrics &metrics,
                              const ComplexityWeights &weights) {
  // Version 2.0: Weighted linear combination of normalized metrics
  // This is the Phase 4 deliverable implementation

  double score = weights.w_spatial * metrics.norm_spatial +
                 weights.w_motion * metrics.norm_motion +
                 weights.w_residual * metrics.norm_residual +
                 weights.w_error * metrics.norm_error;

  // Clamp to [0, 1] range (should already be in range if weights sum to 1.0)
  return std::clamp(score, 0.0, 1.0);
}

void normalizeMetrics(ComplexityMetrics &metrics, int width, int height) {
  int num_pixels = width * height;

  // Normalize each metric
  metrics.norm_spatial =
      normalizeVariance(metrics.spatial_variance, num_pixels);
  metrics.norm_motion =
      normalizeMVMagnitude(metrics.motion_magnitude, width, height);
  metrics.norm_residual = normalizeACEnergy(metrics.ac_energy, num_pixels);
  metrics.norm_error = normalizeMSE(metrics.mse);

  // Compute bits per pixel
  metrics.bits_per_pixel =
      computeBitsPerPixel(metrics.estimated_bits, num_pixels);
}

} // namespace complexity
