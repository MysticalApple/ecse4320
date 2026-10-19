#ifndef KERNELS_H
#define KERNELS_H

#include <stddef.h>

/*
 * Kernel signatures.
 *
 * Each of saxpy/dotprod/elemmul has two versions:
 *
 *   - a *direct* version (no idx) that indexes x/y/z contiguously as
 *     x[i]/y[i]/z[i]. This is the true unit-stride case, and is what the
 *     baseline, locality, alignment, and data-type experiments should use.
 *   - an *_idx* version that indexes through a precomputed array,
 *     touching x[idx[i]]/y[idx[i]]/z[idx[i]]. This is used only for the
 *     stride/gather experiment (idx[i] = i*stride mod n, or a random
 *     permutation for gather -- see datagen.h / build_index_array()).
 *
 * These are kept as separate functions rather than always routing through
 * idx (with idx[i]=i standing in for "unit stride") deliberately: indexing
 * through an array -- even an identity one -- forces the compiler to treat
 * every access as indirect, which in practice suppresses ordinary
 * contiguous-load vectorization and, for some kernels, turns it into a
 * gather/scatter instead. That would silently make the "unit stride"
 * baseline behave like a gather, which defeats the point of comparing the
 * two. See harness/main.c: it calls the direct version when --stride 1
 * (the default) and the _idx version otherwise.
 *
 * stencil3 has no idx parameter at all: a 3-point stencil depends on the
 * order of its neighbors (x[i-1], x[i], x[i+1]), so an arbitrary
 * gather/stride pattern isn't a meaningful variation for it (per the
 * project spec, stride/gather applies "where meaningful"). It always
 * walks unit stride over the interior of the array.
 */

/* y[i] = a*x[i] + y[i] */
void saxpy_f32(size_t n, float  a, const float  *restrict x, float  *restrict y);
void saxpy_f64(size_t n, double a, const double *restrict x, double *restrict y);
void saxpy_f32_idx(size_t n, float  a, const float  *restrict x, float  *restrict y, const size_t *restrict idx);
void saxpy_f64_idx(size_t n, double a, const double *restrict x, double *restrict y, const size_t *restrict idx);

/* returns sum(x[i] * y[i]) */
double dotprod_f32(size_t n, const float  *restrict x, const float  *restrict y);
double dotprod_f64(size_t n, const double *restrict x, const double *restrict y);
double dotprod_f32_idx(size_t n, const float  *restrict x, const float  *restrict y, const size_t *restrict idx);
double dotprod_f64_idx(size_t n, const double *restrict x, const double *restrict y, const size_t *restrict idx);

/* z[i] = x[i] * y[i] */
void elemmul_f32(size_t n, const float  *restrict x, const float  *restrict y, float  *restrict z);
void elemmul_f64(size_t n, const double *restrict x, const double *restrict y, double *restrict z);
void elemmul_f32_idx(size_t n, const float  *restrict x, const float  *restrict y, float  *restrict z, const size_t *restrict idx);
void elemmul_f64_idx(size_t n, const double *restrict x, const double *restrict y, double *restrict z, const size_t *restrict idx);

/* y[i] = a*x[i-1] + b*x[i] + c*x[i+1], for i in [1, n-1) */
void stencil3_f32(size_t n, float  a, float  b, float  c, const float  *restrict x, float  *restrict y);
void stencil3_f64(size_t n, double a, double b, double c, const double *restrict x, double *restrict y);

/*
 * Reference implementations. Identical formulas to the *_idx kernels above
 * (idx[i]=i stands in for unit stride here; these are only ever used for
 * the one-time correctness check, never timed, so that indirection has no
 * cost that matters). Always compiled with -O0 -fno-tree-vectorize (see
 * build/Makefile) so they serve as a trusted, non-vectorized oracle
 * regardless of which variant the harness itself was built with.
 */
void saxpy_f32_ref(size_t n, float  a, const float  *restrict x, float  *restrict y, const size_t *restrict idx);
void saxpy_f64_ref(size_t n, double a, const double *restrict x, double *restrict y, const size_t *restrict idx);

double dotprod_f32_ref(size_t n, const float  *restrict x, const float  *restrict y, const size_t *restrict idx);
double dotprod_f64_ref(size_t n, const double *restrict x, const double *restrict y, const size_t *restrict idx);

void elemmul_f32_ref(size_t n, const float  *restrict x, const float  *restrict y, float  *restrict z, const size_t *restrict idx);
void elemmul_f64_ref(size_t n, const double *restrict x, const double *restrict y, double *restrict z, const size_t *restrict idx);

void stencil3_f32_ref(size_t n, float  a, float  b, float  c, const float  *restrict x, float  *restrict y);
void stencil3_f64_ref(size_t n, double a, double b, double c, const double *restrict x, double *restrict y);

#endif /* KERNELS_H */
