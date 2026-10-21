/* Needed for pread/pwrite/ftruncate/fcntl locks. The Makefile already
 * passes this; the guard just keeps the file self-contained. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "datagen.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* PRNG                                                                */
/* ------------------------------------------------------------------ */

void rng_seed(rng_t *r, uint64_t seed) {
    /* xorshift64 requires a non-zero state. */
    r->state = seed ? seed : 0x9E3779B97F4A7C15ULL;
    r->seed = seed;
    r->fill_pos = 0;
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

/* ------------------------------------------------------------------ */
/* Value stream                                                        */
/* ------------------------------------------------------------------ */

/* Turns the next `count` draws of generator g into [-1, 1) values. This is
 * the one definition of what element k of a seed's stream is; both the
 * cache and the direct path go through it. If you change the formula,
 * bump FORMAT_VERSION below so stale cache files are discarded. */
static void gen_values(rng_t *g, int is_f64, void *dst, size_t count) {
    if (is_f64) {
        double *d = dst;
        for (size_t i = 0; i < count; i++) d[i] = rng_uniform(g) * 2.0 - 1.0;
    } else {
        float *f = dst;
        for (size_t i = 0; i < count; i++) f[i] = (float)(rng_uniform(g) * 2.0 - 1.0);
    }
}

/* No-cache path: regenerate elements [pos, pos+n) of the seed's stream by
 * skipping the first `pos` draws. */
static void generate_direct(void *dst, int is_f64, uint64_t seed, size_t pos, size_t n) {
    rng_t g;
    rng_seed(&g, seed);
    for (size_t i = 0; i < pos; i++) rng_next(&g);
    gen_values(&g, is_f64, dst, n);
}

/* ------------------------------------------------------------------ */
/* On-disk cache                                                       */
/* ------------------------------------------------------------------ */

/*
 * File layout: a 64-byte header followed by `count` raw elements.
 *
 * `end_state` is the generator state after producing those `count`
 * elements, which is what lets a later call extend the file and continue
 * the same stream. The header is rewritten only after new payload has been
 * written, so a run killed mid-extension leaves the old, valid contents
 * (the stray tail bytes are ignored and overwritten next time). There is no
 * fsync: this is safe against interrupted runs, not against power loss.
 */
#define HDR_SIZE 64
#define FORMAT_VERSION 1u

static const char MAGIC[8] = "DGCACHE";

typedef struct {
    char magic[8];
    uint32_t format_version;
    uint32_t elem_size;  /* 4 (f32) or 8 (f64) */
    uint64_t seed;
    uint64_t count;      /* valid elements in the payload */
    uint64_t end_state;  /* generator state after `count` elements */
    uint8_t pad[24];
} cache_header_t;

_Static_assert(sizeof(cache_header_t) == HDR_SIZE, "cache header must be 64 bytes");

/* Set after the first cache failure so we warn once and stop trying. */
static int cache_broken = 0;

static int cache_enabled(void) {
    if (cache_broken) return 0;
    const char *e = getenv("DATAGEN_CACHE");
    return !(e && strcmp(e, "0") == 0);
}

static void warn_once(const char *what, const char *path) {
    fprintf(stderr, "datagen: cache unavailable (%s %s: %s); generating data directly\n",
            what, path, strerror(errno));
    cache_broken = 1;
}

/* pread/pwrite can transfer less than asked (and cap at ~2 GiB per call on
 * Linux), so loop until everything has moved. */
static int pread_full(int fd, void *buf, size_t len, off_t off) {
    char *p = buf;
    while (len > 0) {
        ssize_t k = pread(fd, p, len, off);
        if (k < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (k == 0) { errno = EIO; return -1; } /* unexpected EOF */
        p += k;
        off += k;
        len -= (size_t)k;
    }
    return 0;
}

static int pwrite_full(int fd, const void *buf, size_t len, off_t off) {
    const char *p = buf;
    while (len > 0) {
        ssize_t k = pwrite(fd, p, len, off);
        if (k < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (k == 0) { errno = EIO; return -1; }
        p += k;
        off += k;
        len -= (size_t)k;
    }
    return 0;
}

/* Whole-file advisory lock; type is F_RDLCK, F_WRLCK, or F_UNLCK. */
static int lock_file(int fd, short type) {
    struct flock fl;
    memset(&fl, 0, sizeof fl);
    fl.l_type = type;
    fl.l_whence = SEEK_SET; /* l_start = l_len = 0: the whole file */
    while (fcntl(fd, F_SETLKW, &fl) != 0) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

/* Reads the header into *h. Returns 1 only if it is for this (elem_size,
 * seed) and the file really contains the payload it claims; anything else
 * (new empty file, wrong seed, older format, truncated or damaged file)
 * returns 0 and the caller starts the file over. */
static int header_ok(int fd, cache_header_t *h, uint32_t elem, uint64_t seed) {
    struct stat st;
    if (fstat(fd, &st) != 0) return 0;
    if (pread_full(fd, h, sizeof *h, 0) != 0) return 0;
    if (memcmp(h->magic, MAGIC, sizeof MAGIC) != 0) return 0;
    if (h->format_version != FORMAT_VERSION || h->elem_size != elem || h->seed != seed) return 0;
    if (h->end_state == 0) return 0;
    if (h->count > (UINT64_MAX - HDR_SIZE) / elem) return 0;
    if ((uint64_t)st.st_size < HDR_SIZE + h->count * elem) return 0;
    return 1;
}

/* Empties the file and writes a header for a fresh, zero-element stream. */
static int header_reset(int fd, cache_header_t *h, uint32_t elem, uint64_t seed) {
    rng_t g;
    rng_seed(&g, seed);

    memset(h, 0, sizeof *h);
    memcpy(h->magic, MAGIC, sizeof MAGIC);
    h->format_version = FORMAT_VERSION;
    h->elem_size = elem;
    h->seed = seed;
    h->count = 0;
    h->end_state = g.state;

    if (ftruncate(fd, 0) != 0) return -1;
    return pwrite_full(fd, h, sizeof *h, 0);
}

/* Appends generated elements until the file holds `need`, continuing the
 * stream from h->end_state. Updates *h on success. Caller holds the
 * exclusive lock. */
static int cache_extend(int fd, cache_header_t *h, int is_f64, size_t need) {
    const size_t elem = h->elem_size;
    const size_t chunk = (size_t)1 << 16;
    size_t have = (size_t)h->count;
    int rc = 0;

    void *buf = malloc(chunk * elem);
    if (!buf) return -1;

    rng_t g;
    memset(&g, 0, sizeof g);
    g.state = h->end_state;

    while (have < need) {
        size_t c = need - have;
        if (c > chunk) c = chunk;
        gen_values(&g, is_f64, buf, c);
        if (pwrite_full(fd, buf, c * elem, (off_t)(HDR_SIZE + have * elem)) != 0) {
            rc = -1;
            break;
        }
        have += c;
    }
    free(buf);

    if (rc == 0) {
        /* Header last: until this lands, the old count stays authoritative. */
        cache_header_t nh = *h;
        nh.count = have;
        nh.end_state = g.state;
        rc = pwrite_full(fd, &nh, sizeof nh, 0);
        if (rc == 0) *h = nh;
    }
    if (rc != 0) {
        int e = errno;
        /* Best effort: give back any space the partial tail used. */
        if (ftruncate(fd, (off_t)(HDR_SIZE + (size_t)h->count * elem)) != 0) {
            /* ignore: stray tail bytes are harmless */
        }
        errno = e;
    }
    return rc;
}

/* Reads elements [pos, pos+n) of the seed's stream into dst via the cache
 * file, creating or extending the file first if needed. Returns 0 on
 * success, -1 (after a one-time warning) if the cache can't be used. */
static int cache_fetch(void *dst, int is_f64, uint64_t seed, size_t pos, size_t n) {
    const uint32_t elem = is_f64 ? 8 : 4;
    const size_t need = pos + n;
    const char *dir = getenv("DATAGEN_CACHE_DIR");
    const char *what = "";
    const char *where = NULL; /* what to name in the warning (set once path is built) */
    char path[4096];
    cache_header_t h;
    int fd = -1;

    if (!dir || !*dir) dir = "datagen_cache";

    if (snprintf(path, sizeof path, "%s/rand_%s_seed%llu.bin", dir,
                 is_f64 ? "f64" : "f32", (unsigned long long)seed) >= (int)sizeof path) {
        errno = ENAMETOOLONG;
        what = "path";
        where = dir;
        goto fail;
    }
    where = path;
    if (need < pos || need > (SIZE_MAX - HDR_SIZE) / elem) {
        errno = EOVERFLOW;
        what = "size";
        goto fail;
    }
    if (mkdir(dir, 0777) != 0 && errno != EEXIST) {
        what = "mkdir";
        where = dir;
        goto fail;
    }
    fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        what = "open";
        goto fail;
    }

    /* Fast path: shared lock, so any number of runs can read at once. */
    if (lock_file(fd, F_RDLCK) != 0) { what = "lock"; goto fail; }
    if (header_ok(fd, &h, elem, seed) && h.count >= (uint64_t)need) {
        if (pread_full(fd, dst, n * elem, (off_t)(HDR_SIZE + pos * elem)) != 0) {
            what = "read";
            goto fail;
        }
        close(fd);
        return 0;
    }

    /* Slow path. Drop the shared lock before asking for the exclusive one
     * (converting could deadlock two processes that both want to write),
     * then re-check: someone else may have extended the file meanwhile. */
    if (lock_file(fd, F_UNLCK) != 0) { what = "unlock"; goto fail; }
    if (lock_file(fd, F_WRLCK) != 0) { what = "lock"; goto fail; }
    if (!header_ok(fd, &h, elem, seed)) {
        if (header_reset(fd, &h, elem, seed) != 0) { what = "init"; goto fail; }
    }
    if (h.count < (uint64_t)need) {
        fprintf(stderr, "datagen: %s %s (%llu -> %zu elements)\n",
                h.count ? "extending" : "creating", path,
                (unsigned long long)h.count, need);
        if (cache_extend(fd, &h, is_f64, need) != 0) { what = "write"; goto fail; }
    }
    if (pread_full(fd, dst, n * elem, (off_t)(HDR_SIZE + pos * elem)) != 0) {
        what = "read";
        goto fail;
    }
    close(fd); /* also releases the lock */
    return 0;

fail:
    {
        int e = errno;
        if (fd >= 0) close(fd);
        errno = e;
        warn_once(what, where ? where : dir);
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Public fill functions                                               */
/* ------------------------------------------------------------------ */

static void fill_stream(void *dst, int is_f64, size_t n, rng_t *r) {
    size_t pos = (size_t)r->fill_pos;
    r->fill_pos += n;
    if (n == 0) return;
    if (cache_enabled() && cache_fetch(dst, is_f64, r->seed, pos, n) == 0) return;
    generate_direct(dst, is_f64, r->seed, pos, n);
}

void fill_random_f32(float *a, size_t n, rng_t *r) {
    fill_stream(a, 0, n, r);
}

void fill_random_f64(double *a, size_t n, rng_t *r) {
    fill_stream(a, 1, n, r);
}

/* ------------------------------------------------------------------ */
/* Index arrays (not cached)                                           */
/* ------------------------------------------------------------------ */

void build_index_array(size_t *idx, size_t n, rng_t *r) {
    if (n == 0) return;

    /* Gather: start as identity, then Fisher-Yates shuffle. */
    for (size_t i = 0; i < n; i++) idx[i] = i;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(rng_uniform(r) * (double)(i + 1));
        if (j > i) j = i; /* guard against the rare rounding case */
        size_t tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }
}
