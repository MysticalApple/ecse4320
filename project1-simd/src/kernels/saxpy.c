#include "kernels.h"

/* Direct / unit-stride: contiguous access, what the compiler can actually
 * turn into packed vector loads+stores. Used by every experiment except
 * the stride/gather sweep. */
void saxpy_f32(size_t n, float a, const float *restrict x, float *restrict y) {
    for (size_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}

void saxpy_f64(size_t n, double a, const double *restrict x, double *restrict y) {
    for (size_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}

/* Strided: used when n > stride > 1 */
void saxpy_f32_stride(size_t n, float a, const float *restrict x, float *restrict y, size_t s) {
    for (size_t start = 0; start < s && start < n; start++) {
        for (size_t i = start; i < n; i += s) {
            y[i] = a * x[i] + y[i];
        }
    }
}

void saxpy_f64_stride(size_t n, double a, const double *restrict x, double *restrict y, size_t s) {
    for (size_t start = 0; start < s && start < n; start++) {
        for (size_t i = start; i < n; i += s) {
            y[i] = a * x[i] + y[i];
        }
    }
}

/* Indexed: used only when --stride = 0 (gather access). */
void saxpy_f32_idx(size_t n, float a, const float *restrict x, float *restrict y, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        y[j] = a * x[j] + y[j];
    }
}

void saxpy_f64_idx(size_t n, double a, const double *restrict x, double *restrict y, const size_t *restrict idx) {
    for (size_t i = 0; i < n; i++) {
        size_t j = idx[i];
        y[j] = a * x[j] + y[j];
    }
}
