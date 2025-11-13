
/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "moments.h"

#include "cpu.h"
#include "cpu_disp.h"

#if defined(_X86) || defined(_X64)

#ifdef USE_HIGHWAY_SIMD
// When USE_HIGHWAY_SIMD is enabled, use Highway implementations directly
// Highway has its own dynamic dispatch mechanism

int fastSAD16(FAST_SAD_FORMAL_ARGS) { return fastSAD16_hwy(FAST_SAD_ACTUAL_ARGS); }
int fastSAD8(FAST_SAD_FORMAL_ARGS) { return fastSAD8_hwy(FAST_SAD_ACTUAL_ARGS); }
int fastSAD4(FAST_SAD_FORMAL_ARGS) { return fastSAD4_hwy(FAST_SAD_ACTUAL_ARGS); }

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

#else
// Traditional SSE2 dispatch when Highway is not enabled

IMPL_PROC_1(int, fastSAD16, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS),
            sse2)
IMPL_PROC_1(int, fastSAD8, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fastSAD4, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS), sse2)

IMPL_PROC_1(int, fast_variance16, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fast_variance8, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fast_variance4, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS), sse2)

IMPL_PROC_1(int, fast_calc_mse16, (FAST_MSE_FORMAL_ARGS),
            (FAST_MSE_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fast_calc_mse8, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS),
            sse2)
IMPL_PROC_1(int, fast_calc_mse4, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS),
            sse2)

IMPL_PROC_1(int, fast_bidir_mse16, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fast_bidir_mse8, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS), sse2)
IMPL_PROC_1(int, fast_bidir_mse4, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS), sse2)

#endif  // USE_HIGHWAY_SIMD

#elif defined(_ARM) || defined(_ARM64)

IMPL_PROC_0(int, fastSAD16, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))
IMPL_PROC_0(int, fastSAD8, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))
IMPL_PROC_0(int, fastSAD4, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_variance16, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_variance8, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_variance4, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_calc_mse16, (FAST_MSE_FORMAL_ARGS),
            (FAST_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_calc_mse8, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_calc_mse4, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_bidir_mse16, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_bidir_mse8, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_bidir_mse4, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))

#else // generic platform

IMPL_PROC_0(int, fastSAD16, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))
IMPL_PROC_0(int, fastSAD8, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))
IMPL_PROC_0(int, fastSAD4, (FAST_SAD_FORMAL_ARGS), (FAST_SAD_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_variance16, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_variance8, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_variance4, (FAST_VARIANCE_FORMAL_ARGS),
            (FAST_VARIANCE_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_calc_mse16, (FAST_MSE_FORMAL_ARGS),
            (FAST_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_calc_mse8, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_calc_mse4, (FAST_MSE_FORMAL_ARGS), (FAST_MSE_ACTUAL_ARGS))

IMPL_PROC_0(int, fast_bidir_mse16, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_bidir_mse8, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))
IMPL_PROC_0(int, fast_bidir_mse4, (FAST_BIDIR_MSE_FORMAL_ARGS),
            (FAST_BIDIR_MSE_ACTUAL_ARGS))

#endif
