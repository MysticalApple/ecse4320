#ifndef DATAGEN_H
#define DATAGEN_H

#include <stddef.h>
#include <stdint.h>

/* Small, deterministic xorshift64 PRNG. Not cryptographic -- just needs to
 * be fast, seedable, and reproducible across runs for a given --seed. */
typedef struct {
    uint64_t state;    /* xorshift64 state, advanced by rng_next()/rng_uniform() */
    uint64_t seed;     /* value given to rng_seed(); keys the on-disk cache */
    uint64_t fill_pos; /* how many elements fill_random_*() has handed out so far */
} rng_t;

void rng_seed(rng_t *r, uint64_t seed);
uint64_t rng_next(rng_t *r);
double rng_uniform(rng_t *r); /* uniform double in [0, 1) */

/*
 * Fill with pseudo-random values in [-1, 1), avoiding zero/denormal-prone
 * ranges so kernels do real floating-point work without overflow risk.
 *
 * Each seed defines one fixed stream of values per data type: element k is
 * the k-th draw of a generator started at that seed, mapped to [-1, 1).
 * A call hands out the *next* n elements of the rng's stream (tracked in
 * r->fill_pos), so consecutive calls on one rng -- e.g. x, then y -- get
 * different, non-overlapping data. The stream is independent of r->state,
 * so rng_next()/rng_uniform()/build_index_array() can be interleaved freely
 * without changing what fill_random_*() produces.
 *
 * Caching: the stream is cached on disk, one file per (type, seed):
 *
 *     <dir>/rand_f32_seed<seed>.bin     <dir>/rand_f64_seed<seed>.bin
 *
 * A call reads its slice from the file if the file already holds enough
 * elements; otherwise the file is created, or extended from where it left
 * off (so one file serves a whole sweep over n, and a small n is just a
 * prefix of a big one). Cached and freshly generated values are identical.
 *
 *   DATAGEN_CACHE_DIR  cache directory (default "datagen_cache", relative
 *                      to the current working directory; created if
 *                      missing, but its parent must exist)
 *   DATAGEN_CACHE=0    disable caching, always generate directly
 *
 * If the cache can't be used (unwritable directory, disk full, ...), one
 * warning is printed to stderr and data is generated directly instead.
 * Safe to share between concurrent processes (advisory fcntl locks) and
 * between build variants. Files are native-endian: delete the directory if
 * it was produced on a different machine type or by a different generator.
 */
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
 *
 * Not cached; the gather case consumes draws from r->state.
 */
void build_index_array(size_t *idx, size_t n, long stride, rng_t *r);

#endif /* DATAGEN_H */
