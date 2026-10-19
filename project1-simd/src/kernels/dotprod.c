#include "kernels.h"

/*
 * NOTE on the negative example: a single scalar accumulator creates a
 * loop-carried dependency chain. Floating-point addition is not
 * associative, so under strict IEEE semantics (no -ffast-math /
 * -fassociative-math, which we deliberately do not use anywhere in this
 * project) most compilers will NOT auto-vectorize this reduction at -O3,
 * even in the "autovec_widest" build. This is intended to be the
 * project's required negative example: dotprod_f32/f64 (the direct,
 * unit-stride versions used by the baseline/locality/alignment/dtype
 * experiments) look SIMD-friendly but, when checked with the
 * vectorization-report/disassembly step, are expected to show little or
 * no packed vector use compared to saxpy/elemmul/stencil3. That should be
 * verified empirically, not assumed -- see
 * scripts/verify_vectorization.sh (added alongside the analysis scripts).
 */

double dotprod_f32(size_t n, const float *restrict x, const float *restrict y) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum += x[i] * y[i];
    }
    return (double)sum;
}

double dotprod_f64(size_t n, const double *restrict x, const double *restrict y) {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += x[i] * y[i];
    }
    return sum;
}

double dotprod_f32_idx(size_t n, const float *restrict x, const float *restrict y, const size_t *restrict idx) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        sum += x[j] * y[j];
    }
    return (double)sum;
}

double dotprod_f64_idx(size_t n, const double *restrict x, const double *restrict y, const size_t *restrict idx) {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        sum += x[j] * y[j];
    }
    return sum;
}
