#include "kernels.h"

/* Interior points only (i = 1 .. n-2); boundary elements of y are left
 * untouched by design, so the caller must not read y[0] or y[n-1] as
 * meaningful output. This keeps the kernel free of boundary-condition
 * branching, which would otherwise complicate vectorization for reasons
 * unrelated to what this experiment is trying to measure. */

void stencil3_f32(size_t n, float a, float b, float c, const float *restrict x, float *restrict y) {
    for (size_t i = 1; i + 1 < n; i++) {
        y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
    }
}

void stencil3_f64(size_t n, double a, double b, double c, const double *restrict x, double *restrict y) {
    for (size_t i = 1; i + 1 < n; i++) {
        y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
    }
}

/* Strided: used when n > stride > 1 */
void stencil3_f32_stride(size_t n, float a, float b, float c, const float *restrict x, float *restrict y, size_t s) {
    switch (s) {
    case 2:
        for (size_t start = 0; start < 2 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 2) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    case 4:
        for (size_t start = 0; start < 4 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 4) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    case 8:
        for (size_t start = 0; start < 8 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 8) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    default:
        for (size_t start = 0; start < s && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += s) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    }
}

void stencil3_f64_stride(size_t n, double a, double b, double c, const double *restrict x, double *restrict y, size_t s) {
    switch (s) {
    case 2:
        for (size_t start = 0; start < 2 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 2) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    case 4:
        for (size_t start = 0; start < 4 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 4) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    case 8:
        for (size_t start = 0; start < 8 && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += 8) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    default:
        for (size_t start = 0; start < s && start < n; start++) {
            for (size_t i = start + 1; i + 1 < n; i += s) {
                y[i] = a * x[i - 1] + b * x[i] + c * x[i + 1];
            }
        }
        break;
    }
}

