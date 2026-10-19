#include "datagen.h"

void rng_seed(rng_t *r, uint64_t seed) {
    /* xorshift64 requires a non-zero state. */
    r->state = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

uint64_t rng_next(rng_t *r) {
    uint64_t x = r->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->state = x;
    return x;
}

double rng_uniform(rng_t *r) {
    /* Top 53 bits -> double in [0, 1), same trick used by most xorshift
     * "convert to double" helpers. */
    return (double)(rng_next(r) >> 11) * (1.0 / 9007199254740992.0);
}

void fill_random_f32(float *a, size_t n, rng_t *r) {
    for (size_t i = 0; i < n; i++) {
        a[i] = (float)(rng_uniform(r) * 2.0 - 1.0);
    }
}

void fill_random_f64(double *a, size_t n, rng_t *r) {
    for (size_t i = 0; i < n; i++) {
        a[i] = rng_uniform(r) * 2.0 - 1.0;
    }
}

void build_index_array(size_t *idx, size_t n, long stride, rng_t *r) {
    if (n == 0) return;

    if (stride <= 0) {
        /* Gather: start as identity, then Fisher-Yates shuffle. */
        for (size_t i = 0; i < n; i++) idx[i] = i;
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = (size_t)(rng_uniform(r) * (double)(i + 1));
            if (j > i) j = i; /* guard against the rare rounding case */
            size_t tmp = idx[i];
            idx[i] = idx[j];
            idx[j] = tmp;
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            idx[i] = ((size_t)stride * i) % n;
        }
    }
}
