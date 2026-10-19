#include "kernels.h"

/*
 * Scalar reference implementations, identical in formula to the kernels in
 * src/kernels/.
 * These are always compiled with -O0 -fno-tree-vectorize by build/Makefile,
 * regardless of which variant (scalar / autovec_default / autovec_widest)
 * the rest of the harness was built with. They exist purely so every build
 * has a trusted, non-vectorized oracle to check the kernel-under-test
 * against at runtime (see run_correctness_check() in harness/main.c).
 */

void saxpy_f32_ref(size_t n, float a, const float *restrict x, float *restrict y, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        y[j] = a * x[j] + y[j];
    }
}

void saxpy_f64_ref(size_t n, double a, const double *restrict x, double *restrict y, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        y[j] = a * x[j] + y[j];
    }
}

double dotprod_f32_ref(size_t n, const float *restrict x, const float *restrict y, const size_t *restrict idx) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        sum += x[j] * y[j];
    }
    return (double)sum;
}

double dotprod_f64_ref(size_t n, const double *restrict x, const double *restrict y, const size_t *restrict idx) {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        sum += x[j] * y[j];
    }
    return sum;
}

void elemmul_f32_ref(size_t n, const float *restrict x, const float *restrict y, float *restrict z, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        z[j] = x[j] * y[j];
    }
}

void elemmul_f64_ref(size_t n, const double *restrict x, const double *restrict y, double *restrict z, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        z[j] = x[j] * y[j];
    }
}

void stencil3_f32_ref(size_t n, float a, float b, float c, const float *restrict x, float *restrict y) {
    for (size_t i = 1; i + 1 < n; i++) {
        y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
    }
}

void stencil3_f64_ref(size_t n, double a, double b, double c, const double *restrict x, double *restrict y) {
    for (size_t i = 1; i + 1 < n; i++) {
        y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
    }
}
