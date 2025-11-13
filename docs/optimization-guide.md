# SIMD Optimization Guide (Highway-based)

## Introduction

This project uses [Google Highway](https://github.com/google/highway) for portable, cross-platform SIMD optimizations. Highway provides a single API that compiles to optimal code for multiple instruction sets (SSE2, AVX2, AVX-512, NEON, etc.) and automatically dispatches to the best available implementation at runtime.

## Architecture Overview

An optimized function in this codebase typically consists of:

1. **Main function** - The public API that applications call (e.g., `fastSAD16`)
2. **C reference implementation** - Portable C code used for testing and as a fallback (e.g., `fastSAD16_c`)
3. **Highway SIMD implementation** - Multi-target implementation using Highway's portable API (e.g., `fastSAD16_hwy`)
4. **Dispatcher wrapper** - Simple wrapper that calls the Highway implementation (in `moments.disp.cpp`)

### Example Function Flow

```
Application calls: fastSAD16()
         ↓
Dispatcher (moments.disp.cpp): fastSAD16() → calls fastSAD16_hwy()
         ↓
Highway multi-target (moments.highway.cpp): fastSAD16_hwy()
         ↓
Highway runtime dispatch selects best target:
    → x86: SSE2, AVX2, or AVX-512
    → ARM: NEON
    → Others: Portable SIMD
```

## How to Write SIMD Optimizations with Highway

### Step 1: Declare Function Signatures

In the header file (e.g., `moments.h`), declare your functions:

```c
// Define parameter macros for consistency
#define FAST_SAD_FORMAL_ARGS                                                   \
  const uint8_t *current, const uint8_t *reference, const ptrdiff_t stride,    \
      int block_width, int block_height, int min_SAD

#define FAST_SAD_ACTUAL_ARGS                                                   \
  current, reference, stride, block_width, block_height, min_SAD

// Public API
int fastSAD16(FAST_SAD_FORMAL_ARGS);

// C reference implementation
int fastSAD16_c(FAST_SAD_FORMAL_ARGS);

// Highway SIMD implementation
int fastSAD16_hwy(FAST_SAD_FORMAL_ARGS);
```

### Step 2: Implement C Reference Version

Write a portable C implementation for testing and validation:

```c
int fastSAD16_c(const uint8_t *current, const uint8_t *reference,
                const ptrdiff_t stride, int block_width, int block_height,
                int min_SAD) {
  int sad = 0;
  for (int y = 0; y < block_height; y++) {
    for (int x = 0; x < block_width; x++) {
      sad += abs(current[y * stride + x] - reference[y * stride + x]);
    }
  }
  return sad;
}
```

### Step 3: Implement Highway Multi-Target Version

Create a Highway implementation file (e.g., `motion_search/asm/moments.highway.cpp`):

```cpp
// Highway multi-target setup (include once at top of file)
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "motion_search/asm/moments.highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace motion_search {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// Implement your SIMD function here
// This code will be compiled multiple times for different targets
int fastSAD16_hwy_impl(const uint8_t *current, const uint8_t *reference,
                       const ptrdiff_t stride, int block_width,
                       int block_height, int min_SAD) {
  const hn::FixedTag<uint8_t, 16> d;

  int total_sad = 0;

  for (int y = 0; y < block_height; y++) {
    const uint8_t *cur_row = current + y * stride;
    const uint8_t *ref_row = reference + y * stride;

    for (int x = 0; x < block_width; x += 16) {
      // Load 16 bytes
      auto cur_vec = hn::LoadU(d, cur_row + x);
      auto ref_vec = hn::LoadU(d, ref_row + x);

      // Compute absolute differences and sum
      auto sad_vec = hn::AbsDiff(cur_vec, ref_vec);
      total_sad += hn::ReduceSum(d, sad_vec);
    }
  }

  return total_sad;
}

}  // namespace HWY_NAMESPACE
}  // namespace motion_search
HWY_AFTER_NAMESPACE();

// Export function for dynamic dispatch (include once per file)
#if HWY_ONCE
namespace motion_search {

HWY_EXPORT(fastSAD16_hwy_impl);

// Wrapper that dispatches to the best available target
extern "C" int fastSAD16_hwy(const uint8_t *current, const uint8_t *reference,
                             const ptrdiff_t stride, int block_width,
                             int block_height, int min_SAD) {
  return HWY_DYNAMIC_DISPATCH(fastSAD16_hwy_impl)(
      current, reference, stride, block_width, block_height, min_SAD);
}

}  // namespace motion_search
#endif  // HWY_ONCE
```

### Step 4: Create Dispatcher Wrapper

In `moments.disp.cpp`, create a simple wrapper:

```cpp
extern "C" {

int fastSAD16(const uint8_t *current, const uint8_t *reference,
              const ptrdiff_t stride, int block_width, int block_height,
              int min_SAD) {
  return fastSAD16_hwy(current, reference, stride, block_width, block_height,
                       min_SAD);
}

}  // extern "C"
```

## Common Highway Patterns

### Loading Data

```cpp
const hn::FixedTag<uint8_t, 16> d;  // 16-byte vectors
auto vec = hn::LoadU(d, ptr);       // Unaligned load
auto vec = hn::Load(d, ptr);        // Aligned load (faster if guaranteed aligned)
```

### Arithmetic Operations

```cpp
auto sum = hn::Add(a, b);           // Vector addition
auto diff = hn::Sub(a, b);          // Vector subtraction
auto product = hn::Mul(a, b);       // Vector multiplication
auto abs_diff = hn::AbsDiff(a, b);  // |a - b|
auto avg = hn::AverageRound(a, b);  // (a + b + 1) / 2
```

### Type Conversions

```cpp
// Widen lower/upper halves of uint8_t to uint16_t
const hn::FixedTag<uint8_t, 16> d8;
const hn::FixedTag<uint16_t, 8> d16;

auto vec8 = hn::LoadU(d8, ptr);
auto lower16 = hn::PromoteLowerTo(d16, vec8);
auto upper16 = hn::PromoteUpperTo(d16, vec8);
```

### Reductions

```cpp
auto sum = hn::ReduceSum(d, vec);   // Horizontal sum of all elements
```

### Shifts

```cpp
auto shifted = hn::ShiftRight<4>(vec);  // Shift right by 4 bits
```

## Best Practices

1. **Use Fixed-Size Vectors**: For algorithms that process specific block sizes (8x8, 16x16), use `FixedTag<T, N>` for predictable performance.

2. **Handle Remainders**: If your data size isn't a multiple of the vector size, handle the remainder with scalar code or masked operations.

3. **Minimize Horizontal Operations**: Operations like `ReduceSum` are slower than vertical operations. Accumulate in vectors when possible.

4. **Test Against Reference**: Always validate your Highway implementation against the C reference using the test suite.

5. **Profile**: Highway automatically picks the best target, but you should still profile to ensure good performance.

## Highway API Reference

For complete documentation, see:
- [Highway Quick Reference](https://github.com/google/highway/blob/master/g3doc/quick_reference.md)
- [Highway API Documentation](https://github.com/google/highway/blob/master/g3doc/README.md)
- [Porting Guide (from SSE2/NEON)](https://github.com/google/highway/blob/master/g3doc/porting.md)

## Supported Targets

Highway automatically compiles and dispatches to these targets:

### x86/x86-64
- SSE2 (baseline, always available on x86-64)
- SSSE3
- SSE4.1
- AVX2
- AVX-512

### ARM
- NEON (ARMv7-A and ARMv8-A)
- SVE (ARMv8-A with SVE)

### Other Architectures
- WASM SIMD
- RISC-V Vector Extension
- PowerPC VSX
- Portable fallback (EMU128 for any platform)

## Example: Existing Implementations

See these files for complete examples:
- `motion_search/asm/moments.highway.cpp` - All SIMD primitive implementations
- `motion_search/moments.disp.cpp` - Dispatcher wrappers
- `motion_search/moments.h` - Function declarations
- `tests/test_moments.cpp` - Test suite validating implementations

## Migration from SSE2

If you have existing SSE2 code, refer to the [Highway Porting Guide](https://github.com/google/highway/blob/master/g3doc/porting.md) for common intrinsic mappings. Key differences:

| SSE2 Intrinsic | Highway Equivalent |
|----------------|-------------------|
| `_mm_load_si128()` | `Load(d, ptr)` |
| `_mm_loadu_si128()` | `LoadU(d, ptr)` |
| `_mm_add_epi8()` | `Add(a, b)` |
| `_mm_sub_epi8()` | `Sub(a, b)` |
| `_mm_sad_epu8()` | `AbsDiff()` + `ReduceSum()` |
| `_mm_mullo_epi16()` | `Mul(a, b)` |
| `_mm_setzero_si128()` | `Zero(d)` |

Highway handles type safety automatically, making many explicit type casts unnecessary.
