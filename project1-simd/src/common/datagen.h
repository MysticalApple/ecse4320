#ifndef DATAGEN_H
#define DATAGEN_H

#include <stddef.h>
#include <stdint.h>

/* Small, deterministic xorshift64 PRNG. Not cryptographic -- just needs to
 * be fast, seedable, and reproducible across runs for a given --seed. */
typedef struct {
    uint64_t state;
} rng_t;

void rng_seed(rng_t *r, uint64_t seed);
uint64_t rng_next(rng_t *r);
double rng_uniform(rng_t *r); /* uniform double in [0, 1) */

/* Fill with pseudo-random values in [-1, 1), avoiding zero/denormal-prone
 * ranges so kernels do real floating-point work without overflow risk. */
void fill_random_f32(float *a, size_t n, rng_t *r);
void fill_random_f64(double *a, size_t n, rng_t *r);

/*
 * Builds the access-pattern index array used by saxpy/dotprod/elemmul.
 *
 *   stride <= 0 : gather pattern -- idx is a random permutation of
 *                 [0, n) (Fisher-Yates), i.e. fully unpredictable access.
 *   stride >= 1 : idx[i] = (i * stride) % n -- unit stride when stride==1,
 *                 strided access for stride > 1. The modulo wrap keeps
 *                 every access in-bounds without needing an oversized
 *                 allocation; since idx is precomputed outside the timed
 *                 region, the wrap-around has no effect on what's being
 *                 measured (effective bandwidth/latency of that stride).
 */
void build_index_array(size_t *idx, size_t n, long stride, rng_t *r);

#endif /* DATAGEN_H */
