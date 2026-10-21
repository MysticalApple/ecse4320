#include "kernels.h"

void elemmul_f32(size_t n, const float *restrict x, const float *restrict y, float *restrict z) {
    for (size_t i = 0; i < n; i++) {
        z[i] = x[i] * y[i];
    }
}

void elemmul_f64(size_t n, const double *restrict x, const double *restrict y, double *restrict z) {
    for (size_t i = 0; i < n; i++) {
        z[i] = x[i] * y[i];
    }
}

/* Strided: used when n > stride > 1 */
void elemmul_f32_stride(size_t n, const float *restrict x, const float *restrict y, float *restrict z, size_t s) {
    for (size_t start = 0; start < s && start < n; start++) {
        for (size_t i = start; i < n; i += s) {
            z[i] = x[i] * y[i];
        }
    }
}

void elemmul_f64_stride(size_t n, const double *restrict x, const double *restrict y, float *restrict z, size_t s) {
    for (size_t start = 0; start < s && start < n; start++) {
        for (size_t i = start; i < n; i += s) {
            z[i] = x[i] * y[i];
        }
    }
}

void elemmul_f32_idx(size_t n, const float *restrict x, const float *restrict y, float *restrict z, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        z[j] = x[j] * y[j];
    }
}

void elemmul_f64_idx(size_t n, const double *restrict x, const double *restrict y, double *restrict z, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        z[j] = x[j] * y[j];
    }
}
