
/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "moments.h"

// Simple wrapper functions that dispatch to Highway SIMD implementations
// Highway handles target-specific dispatch internally

int fastSAD16(FAST_SAD_FORMAL_ARGS) {
  return fastSAD16_hwy(FAST_SAD_ACTUAL_ARGS);
}

int fastSAD8(FAST_SAD_FORMAL_ARGS) {
  return fastSAD8_hwy(FAST_SAD_ACTUAL_ARGS);
}

int fastSAD4(FAST_SAD_FORMAL_ARGS) {
  return fastSAD4_hwy(FAST_SAD_ACTUAL_ARGS);
}

int fast_variance16(FAST_VARIANCE_FORMAL_ARGS) {
  return fast_variance16_hwy(FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_variance8(FAST_VARIANCE_FORMAL_ARGS) {
  return fast_variance8_hwy(FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_variance4(FAST_VARIANCE_FORMAL_ARGS) {
  return fast_variance4_hwy(FAST_VARIANCE_ACTUAL_ARGS);
}

int fast_calc_mse16(FAST_MSE_FORMAL_ARGS) {
  return fast_calc_mse16_hwy(FAST_MSE_ACTUAL_ARGS);
}

int fast_calc_mse8(FAST_MSE_FORMAL_ARGS) {
  return fast_calc_mse8_hwy(FAST_MSE_ACTUAL_ARGS);
}

int fast_calc_mse4(FAST_MSE_FORMAL_ARGS) {
  return fast_calc_mse4_hwy(FAST_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse16(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return fast_bidir_mse16_hwy(FAST_BIDIR_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse8(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return fast_bidir_mse8_hwy(FAST_BIDIR_MSE_ACTUAL_ARGS);
}

int fast_bidir_mse4(FAST_BIDIR_MSE_FORMAL_ARGS) {
  return fast_bidir_mse4_hwy(FAST_BIDIR_MSE_ACTUAL_ARGS);
}
