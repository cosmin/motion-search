/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

// Highway implementation of SIMD moments functions
// This file uses Highway's multi-target mechanism for portable SIMD

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "motion_search/asm/moments.highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "motion_search/moments.h"

HWY_BEFORE_NAMESPACE();
namespace motion_search {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Helper function to perform horizontal sum of a vector
template <class D, class V>
HWY_INLINE int HorizontalSum(D d, V v) {
  const size_t N = hn::Lanes(d);
  HWY_ALIGN int32_t lanes[HWY_MAX_LANES_D(D)];
  hn::Store(v, d, lanes);
  int sum = 0;
  for (size_t i = 0; i < N; ++i) {
    sum += lanes[i];
  }
  return sum;
}

// SAD (Sum of Absolute Differences) - 16 byte width
int fastSAD16_highway(FAST_SAD_FORMAL_ARGS) {
  UNUSED(block_width);
  UNUSED(min_SAD);

  const hn::FixedTag<uint8_t, 16> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;

  auto sum16 = hn::Zero(d16);

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);
    // AbsDiff computes |a - b| for each lane
    auto diff = hn::AbsDiff(curr, ref);

    // Promote to 16-bit and accumulate
    auto diff_lo = hn::PromoteLowerTo(d16, diff);
    auto diff_hi = hn::PromoteUpperTo(d16, diff);
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);

    current += stride;
    reference += stride;
  }

  return static_cast<int>(hn::ReduceSum(d16, sum16));
}

// SAD - 8 byte width
int fastSAD8_highway(FAST_SAD_FORMAL_ARGS) {
  UNUSED(block_width);
  UNUSED(min_SAD);

  const hn::FixedTag<uint8_t, 8> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;

  auto sum16 = hn::Zero(d16);

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);
    auto diff = hn::AbsDiff(curr, ref);

    // Split and promote 8 bytes -> 2x 4 uint16
    auto diff_lo = hn::PromoteLowerTo(d16, diff);
    auto diff_hi = hn::PromoteUpperTo(d16, diff);
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);

    current += stride;
    reference += stride;
  }

  return static_cast<int>(hn::ReduceSum(d16, sum16));
}

// SAD - 4 byte width
int fastSAD4_highway(FAST_SAD_FORMAL_ARGS) {
  UNUSED(block_width);
  UNUSED(min_SAD);

  const hn::FixedTag<uint8_t, 4> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;

  auto sum16 = hn::Zero(d16);

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);
    auto diff = hn::AbsDiff(curr, ref);

    // Split and promote 4 bytes -> 2x 2 uint16
    auto diff_lo = hn::PromoteLowerTo(d16, diff);
    auto diff_hi = hn::PromoteUpperTo(d16, diff);
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);

    current += stride;
    reference += stride;
  }

  return static_cast<int>(hn::ReduceSum(d16, sum16));
}

// Variance - 16 byte width
int fast_variance16_highway(FAST_VARIANCE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 16> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;
  const hn::Repartition<uint32_t, decltype(d)> d32;

  auto sum16 = hn::Zero(d16);
  auto sum32 = hn::Zero(d32);

  for (int i = block_height; i > 0; i--) {
    auto pixels = hn::LoadU(d, current);

    // Split into 16-bit for sum
    auto pixels_lo = hn::PromoteLowerTo(d16, pixels);
    auto pixels_hi = hn::PromoteUpperTo(d16, pixels);
    sum16 = hn::Add(sum16, pixels_lo);
    sum16 = hn::Add(sum16, pixels_hi);

    // Convert to 32-bit and square for sum of squares
    auto lo32_lo = hn::PromoteLowerTo(d32, pixels_lo);
    auto lo32_hi = hn::PromoteUpperTo(d32, pixels_lo);
    auto hi32_lo = hn::PromoteLowerTo(d32, pixels_hi);
    auto hi32_hi = hn::PromoteUpperTo(d32, pixels_hi);

    sum32 = hn::Add(sum32, hn::Mul(lo32_lo, lo32_lo));
    sum32 = hn::Add(sum32, hn::Mul(lo32_hi, lo32_hi));
    sum32 = hn::Add(sum32, hn::Mul(hi32_lo, hi32_lo));
    sum32 = hn::Add(sum32, hn::Mul(hi32_hi, hi32_hi));

    current += stride;
  }

  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

  int temp = block_height << 4;
  return sum2 - (sum * sum + (temp >> 1)) / temp;
}

// Variance - 8 byte width
int fast_variance8_highway(FAST_VARIANCE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 8> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;
  const hn::Repartition<uint32_t, decltype(d)> d32;

  auto sum16 = hn::Zero(d16);
  auto sum32 = hn::Zero(d32);

  for (int i = block_height; i > 0; i--) {
    auto pixels = hn::LoadU(d, current);

    // Split and promote 8 bytes -> 2x 4 uint16
    auto pixels_lo = hn::PromoteLowerTo(d16, pixels);
    auto pixels_hi = hn::PromoteUpperTo(d16, pixels);
    sum16 = hn::Add(sum16, pixels_lo);
    sum16 = hn::Add(sum16, pixels_hi);

    // Promote to 32-bit and square
    auto lo32_lo = hn::PromoteLowerTo(d32, pixels_lo);
    auto lo32_hi = hn::PromoteUpperTo(d32, pixels_lo);
    auto hi32_lo = hn::PromoteLowerTo(d32, pixels_hi);
    auto hi32_hi = hn::PromoteUpperTo(d32, pixels_hi);

    sum32 = hn::Add(sum32, hn::Mul(lo32_lo, lo32_lo));
    sum32 = hn::Add(sum32, hn::Mul(lo32_hi, lo32_hi));
    sum32 = hn::Add(sum32, hn::Mul(hi32_lo, hi32_lo));
    sum32 = hn::Add(sum32, hn::Mul(hi32_hi, hi32_hi));

    current += stride;
  }

  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

  int temp = block_height << 3;
  return sum2 - (sum * sum + (temp >> 1)) / temp;
}

// Variance - 4 byte width
int fast_variance4_highway(FAST_VARIANCE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 4> d;
  const hn::Repartition<uint16_t, decltype(d)> d16;
  const hn::Repartition<uint32_t, decltype(d)> d32;

  auto sum16 = hn::Zero(d16);
  auto sum32 = hn::Zero(d32);

  for (int i = block_height; i > 0; i--) {
    auto pixels = hn::LoadU(d, current);

    // Split and promote 4 bytes -> 2x 2 uint16
    auto pixels_lo = hn::PromoteLowerTo(d16, pixels);
    auto pixels_hi = hn::PromoteUpperTo(d16, pixels);
    sum16 = hn::Add(sum16, pixels_lo);
    sum16 = hn::Add(sum16, pixels_hi);

    // Promote to 32-bit and square
    auto lo32_lo = hn::PromoteLowerTo(d32, pixels_lo);
    auto lo32_hi = hn::PromoteUpperTo(d32, pixels_lo);
    auto hi32_lo = hn::PromoteLowerTo(d32, pixels_hi);
    auto hi32_hi = hn::PromoteUpperTo(d32, pixels_hi);

    sum32 = hn::Add(sum32, hn::Mul(lo32_lo, lo32_lo));
    sum32 = hn::Add(sum32, hn::Mul(lo32_hi, lo32_hi));
    sum32 = hn::Add(sum32, hn::Mul(hi32_lo, hi32_lo));
    sum32 = hn::Add(sum32, hn::Mul(hi32_hi, hi32_hi));

    current += stride;
  }

  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

  int temp = block_height << 2;
  return sum2 - (sum * sum + (temp >> 1)) / temp;
}

// MSE (Mean Squared Error) - 16 byte width
int fast_calc_mse16_highway(FAST_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 16> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);

    // Convert to signed 16-bit and compute difference
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);
    auto ref_lo = hn::PromoteLowerTo(d16, ref);
    auto ref_hi = hn::PromoteUpperTo(d16, ref);

    auto diff_lo = hn::Sub(curr_lo, ref_lo);
    auto diff_hi = hn::Sub(curr_hi, ref_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    // Convert to 32-bit and square
    auto diff_lo_lo = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi_lo = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo_lo, diff_lo_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo_hi, diff_lo_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_lo, diff_hi_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_hi, diff_hi_hi));

    current += stride;
    reference += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 4;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

// MSE - 8 byte width
int fast_calc_mse8_highway(FAST_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 8> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);

    // Split and promote 8 bytes -> 2x 4 int16
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);
    auto ref_lo = hn::PromoteLowerTo(d16, ref);
    auto ref_hi = hn::PromoteUpperTo(d16, ref);

    auto diff_lo = hn::Sub(curr_lo, ref_lo);
    auto diff_hi = hn::Sub(curr_hi, ref_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    auto diff_lo_lo = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi_lo = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo_lo, diff_lo_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo_hi, diff_lo_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_lo, diff_hi_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_hi, diff_hi_hi));

    current += stride;
    reference += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 3;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

// MSE - 4 byte width
int fast_calc_mse4_highway(FAST_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 4> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  for (int i = block_height; i > 0; i--) {
    auto curr = hn::LoadU(d, current);
    auto ref = hn::LoadU(d, reference);

    // Split and promote 4 bytes -> 2x 2 int16
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);
    auto ref_lo = hn::PromoteLowerTo(d16, ref);
    auto ref_hi = hn::PromoteUpperTo(d16, ref);

    auto diff_lo = hn::Sub(curr_lo, ref_lo);
    auto diff_hi = hn::Sub(curr_hi, ref_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    auto diff_lo32 = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo32_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi32 = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi32_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo32, diff_lo32));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo32_hi, diff_lo32_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi32, diff_hi32));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi32_hi, diff_hi32_hi));

    current += stride;
    reference += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 2;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

// Bidirectional MSE - 16 byte width
int fast_bidir_mse16_highway(FAST_BIDIR_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 16> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  // Temporal distance weights
  const int16_t td_y = td->y;
  const int16_t td_x = td->x;

  for (int i = block_height; i > 0; i--) {
    auto ref1 = hn::LoadU(d, reference1);
    auto ref2 = hn::LoadU(d, reference2);

    // Promote to 16-bit for interpolation
    auto ref1_lo = hn::PromoteLowerTo(d16, ref1);
    auto ref1_hi = hn::PromoteUpperTo(d16, ref1);
    auto ref2_lo = hn::PromoteLowerTo(d16, ref2);
    auto ref2_hi = hn::PromoteUpperTo(d16, ref2);

    // Weighted interpolation: (ref1 * td_y + ref2 * td_x + 16384) >> 15
    auto td_y_vec = hn::Set(d16, td_y);
    auto td_x_vec = hn::Set(d16, td_x);

    auto interp_lo = hn::Add(hn::Mul(ref1_lo, td_y_vec),
                              hn::Mul(ref2_lo, td_x_vec));
    auto interp_hi = hn::Add(hn::Mul(ref1_hi, td_y_vec),
                              hn::Mul(ref2_hi, td_x_vec));

    // Add 16384 and shift right by 15
    auto offset = hn::Set(d16, 16384);
    interp_lo = hn::Add(interp_lo, offset);
    interp_hi = hn::Add(interp_hi, offset);
    interp_lo = hn::ShiftRight<15>(interp_lo);
    interp_hi = hn::ShiftRight<15>(interp_hi);

    // Load current and compute difference
    auto curr = hn::LoadU(d, current);
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);

    auto diff_lo = hn::Sub(interp_lo, curr_lo);
    auto diff_hi = hn::Sub(interp_hi, curr_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    // Convert to 32-bit and square
    auto diff_lo_lo = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi_lo = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo_lo, diff_lo_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo_hi, diff_lo_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_lo, diff_hi_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_hi, diff_hi_hi));

    current += stride;
    reference1 += stride;
    reference2 += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 4;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

// Bidirectional MSE - 8 byte width
int fast_bidir_mse8_highway(FAST_BIDIR_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 8> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  const int16_t td_y = td->y;
  const int16_t td_x = td->x;

  for (int i = block_height; i > 0; i--) {
    auto ref1 = hn::LoadU(d, reference1);
    auto ref2 = hn::LoadU(d, reference2);

    // Split and promote 8 bytes -> 2x 4 int16
    auto ref1_lo = hn::PromoteLowerTo(d16, ref1);
    auto ref1_hi = hn::PromoteUpperTo(d16, ref1);
    auto ref2_lo = hn::PromoteLowerTo(d16, ref2);
    auto ref2_hi = hn::PromoteUpperTo(d16, ref2);

    auto td_y_vec = hn::Set(d16, td_y);
    auto td_x_vec = hn::Set(d16, td_x);

    auto interp_lo = hn::Add(hn::Mul(ref1_lo, td_y_vec),
                              hn::Mul(ref2_lo, td_x_vec));
    auto interp_hi = hn::Add(hn::Mul(ref1_hi, td_y_vec),
                              hn::Mul(ref2_hi, td_x_vec));

    auto offset = hn::Set(d16, 16384);
    interp_lo = hn::Add(interp_lo, offset);
    interp_hi = hn::Add(interp_hi, offset);
    interp_lo = hn::ShiftRight<15>(interp_lo);
    interp_hi = hn::ShiftRight<15>(interp_hi);

    auto curr = hn::LoadU(d, current);
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);

    auto diff_lo = hn::Sub(interp_lo, curr_lo);
    auto diff_hi = hn::Sub(interp_hi, curr_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    auto diff_lo_lo = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi_lo = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo_lo, diff_lo_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo_hi, diff_lo_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_lo, diff_hi_lo));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi_hi, diff_hi_hi));

    current += stride;
    reference1 += stride;
    reference2 += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 3;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

// Bidirectional MSE - 4 byte width
int fast_bidir_mse4_highway(FAST_BIDIR_MSE_FORMAL_ARGS) {
  UNUSED(block_width);

  const hn::FixedTag<uint8_t, 4> d;
  const hn::Repartition<int16_t, decltype(d)> d16;
  const hn::Repartition<int32_t, decltype(d)> d32;

  auto sum32 = hn::Zero(d32);
#ifdef AC_ENERGY
  auto sum16 = hn::Zero(d16);
#endif

  const int16_t td_y = td->y;
  const int16_t td_x = td->x;

  for (int i = block_height; i > 0; i--) {
    auto ref1 = hn::LoadU(d, reference1);
    auto ref2 = hn::LoadU(d, reference2);

    // Split and promote 4 bytes -> 2x 2 int16
    auto ref1_lo = hn::PromoteLowerTo(d16, ref1);
    auto ref1_hi = hn::PromoteUpperTo(d16, ref1);
    auto ref2_lo = hn::PromoteLowerTo(d16, ref2);
    auto ref2_hi = hn::PromoteUpperTo(d16, ref2);

    auto td_y_vec = hn::Set(d16, td_y);
    auto td_x_vec = hn::Set(d16, td_x);

    auto interp_lo = hn::Add(hn::Mul(ref1_lo, td_y_vec),
                              hn::Mul(ref2_lo, td_x_vec));
    auto interp_hi = hn::Add(hn::Mul(ref1_hi, td_y_vec),
                              hn::Mul(ref2_hi, td_x_vec));

    auto offset = hn::Set(d16, 16384);
    interp_lo = hn::Add(interp_lo, offset);
    interp_hi = hn::Add(interp_hi, offset);
    interp_lo = hn::ShiftRight<15>(interp_lo);
    interp_hi = hn::ShiftRight<15>(interp_hi);

    auto curr = hn::LoadU(d, current);
    auto curr_lo = hn::PromoteLowerTo(d16, curr);
    auto curr_hi = hn::PromoteUpperTo(d16, curr);

    auto diff_lo = hn::Sub(interp_lo, curr_lo);
    auto diff_hi = hn::Sub(interp_hi, curr_hi);

#ifdef AC_ENERGY
    sum16 = hn::Add(sum16, diff_lo);
    sum16 = hn::Add(sum16, diff_hi);
#endif

    auto diff_lo32 = hn::PromoteLowerTo(d32, diff_lo);
    auto diff_lo32_hi = hn::PromoteUpperTo(d32, diff_lo);
    auto diff_hi32 = hn::PromoteLowerTo(d32, diff_hi);
    auto diff_hi32_hi = hn::PromoteUpperTo(d32, diff_hi);

    sum32 = hn::Add(sum32, hn::Mul(diff_lo32, diff_lo32));
    sum32 = hn::Add(sum32, hn::Mul(diff_lo32_hi, diff_lo32_hi));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi32, diff_hi32));
    sum32 = hn::Add(sum32, hn::Mul(diff_hi32_hi, diff_hi32_hi));

    current += stride;
    reference1 += stride;
    reference2 += stride;
  }

  int sum2 = static_cast<int>(hn::ReduceSum(d32, sum32));

#ifdef AC_ENERGY
  int sum = static_cast<int>(hn::ReduceSum(d16, sum16));
  int temp = block_height << 2;
  sum2 -= (sum * sum + (temp >> 1)) / temp;
#endif

  return sum2;
}

}  // namespace HWY_NAMESPACE
}  // namespace motion_search
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace motion_search {

// Export functions using Highway's dynamic dispatch
HWY_EXPORT(fastSAD16_highway);
HWY_EXPORT(fastSAD8_highway);
HWY_EXPORT(fastSAD4_highway);
HWY_EXPORT(fast_variance16_highway);
HWY_EXPORT(fast_variance8_highway);
HWY_EXPORT(fast_variance4_highway);
HWY_EXPORT(fast_calc_mse16_highway);
HWY_EXPORT(fast_calc_mse8_highway);
HWY_EXPORT(fast_calc_mse4_highway);
HWY_EXPORT(fast_bidir_mse16_highway);
HWY_EXPORT(fast_bidir_mse8_highway);
HWY_EXPORT(fast_bidir_mse4_highway);

// C-compatible wrapper functions
extern "C" {

int fastSAD16_hwy(FAST_SAD_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fastSAD16_highway)(FAST_SAD_ACTUAL_ARGS);
}

int fastSAD8_hwy(FAST_SAD_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fastSAD8_highway)(FAST_SAD_ACTUAL_ARGS);
}

int fastSAD4_hwy(FAST_SAD_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fastSAD4_highway)(FAST_SAD_ACTUAL_ARGS);
}

int fast_variance16_hwy(FAST_VARIANCE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_variance16_highway)(
      FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_variance8_hwy(FAST_VARIANCE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_variance8_highway)(FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_variance4_hwy(FAST_VARIANCE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_variance4_highway)(FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_calc_mse16_hwy(FAST_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_calc_mse16_highway)(FAST_MSE_ACTUAL_ARGS);
}

int fast_calc_mse8_hwy(FAST_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_calc_mse8_highway)(FAST_MSE_ACTUAL_ARGS);
}

int fast_calc_mse4_hwy(FAST_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_calc_mse4_highway)(FAST_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse16_hwy(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_bidir_mse16_highway)(
      FAST_BIDIR_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse8_hwy(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_bidir_mse8_highway)(
      FAST_BIDIR_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse4_hwy(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return HWY_DYNAMIC_DISPATCH(fast_bidir_mse4_highway)(
      FAST_BIDIR_MSE_ACTUAL_ARGS);
}

}  // extern "C"

}  // namespace motion_search
#endif  // HWY_ONCE
