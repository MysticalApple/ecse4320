/*
 * Project 1 -- SIMD Advantage Profiling harness.
 *
 * One process = one (kernel, dtype, n, align_offset, stride) configuration.
 * Which build variant (scalar / autovec_default / autovec_widest) is in
 * effect is determined entirely at compile time by build/Makefile; the
 * --variant flag below is just a label so it ends up in the CSV output for
 * the Python analysis scripts to group by.
 *
 * Output: one CSV line per repetition on stdout, no header (scripts add
 * the header when concatenating runs). Column order:
 *
 *   kernel,variant,dtype,n,align_offset,stride,rep,time_ns,gflops,gbps,checksum_ok
 *
 * Usage:
 *   harness_<variant> --kernel saxpy --dtype f32 --n 1000000 \
 *       --stride 1 --align-offset 0 --reps 11 --warmup 3 --inner 16 \
 *       --variant autovec_default --seed 12345
 */

#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "datagen.h"
#include "kernels.h"
#include "timing.h"

typedef enum {
    KERNEL_SAXPY,
    KERNEL_DOTPROD,
    KERNEL_ELEMMUL,
    KERNEL_STENCIL3
} kernel_t;
typedef enum { DTYPE_F32, DTYPE_F64 } dtype_t;

/* Fixed kernel coefficients. Values don't matter for performance (no
 * data-dependent branching in any of these kernels) -- they're just
 * nonzero, non-trivial constants so the compiler can't fold anything away. */
const double A_COEF = 1.6180339887;
const double STENCIL_A = 0.25, STENCIL_B = 0.5, STENCIL_C = 0.25;

kernel_t parse_kernel(const char *s) {
    if (strcmp(s, "saxpy") == 0)
        return KERNEL_SAXPY;
    if (strcmp(s, "dotprod") == 0)
        return KERNEL_DOTPROD;
    if (strcmp(s, "elemmul") == 0)
        return KERNEL_ELEMMUL;
    if (strcmp(s, "stencil3") == 0)
        return KERNEL_STENCIL3;

    fprintf(stderr,
            "unknown --kernel '%s' (expected saxpy|dotprod|elemmul|stencil3)\n",
            s);
    exit(1);
}

dtype_t parse_dtype(const char *s) {
    if (strcmp(s, "f32") == 0)
        return DTYPE_F32;
    if (strcmp(s, "f64") == 0)
        return DTYPE_F64;

    fprintf(stderr, "unknown --dtype '%s' (expected f32|f64)\n", s);
    exit(1);
}

const char *kernel_name_of(kernel_t k) {
    switch (k) {
    case KERNEL_SAXPY:
        return "saxpy";
    case KERNEL_DOTPROD:
        return "dotprod";
    case KERNEL_ELEMMUL:
        return "elemmul";
    case KERNEL_STENCIL3:
        return "stencil3";
    }

    return "?";
}

const char *dtype_name_of(dtype_t d) { return d == DTYPE_F32 ? "f32" : "f64"; }

/*
 * Runs the kernel-under-test once. `z` is only used by elemmul (NULL
 * otherwise). `idx` selects which code path runs: NULL means unit stride,
 * so the direct (contiguous) kernel is called; non-NULL means the
 * strided/gather kernel is called instead. stencil3 ignores idx entirely
 * (it's always direct -- see kernels.h). main() only builds idx at all
 * when --stride != 1.
 */
void run_kernel_once(kernel_t kernel, dtype_t dtype, size_t n, void *x, void *y,
                     void *z, const size_t *idx) {
    switch (kernel) {
    case KERNEL_SAXPY:
        if (dtype == DTYPE_F32) {
            if (idx)
                saxpy_f32_idx(n, (float)A_COEF, x, y, idx);
            else
                saxpy_f32(n, (float)A_COEF, x, y);
        } else {
            if (idx)
                saxpy_f64_idx(n, A_COEF, x, y, idx);
            else
                saxpy_f64(n, A_COEF, x, y);
        }
        break;

    case KERNEL_DOTPROD:
        /* volatile forces the result to actually be produced, so the
         * whole loop can't be optimized away as dead code. */
        volatile double s;
        if (dtype == DTYPE_F32)
            s = idx ? dotprod_f32_idx(n, x, y, idx) : dotprod_f32(n, x, y);
        else
            s = idx ? dotprod_f64_idx(n, x, y, idx) : dotprod_f64(n, x, y);
        (void)s;
        break;

    case KERNEL_ELEMMUL:
        if (dtype == DTYPE_F32) {
            if (idx)
                elemmul_f32_idx(n, x, y, z, idx);
            else
                elemmul_f32(n, x, y, z);
        } else {
            if (idx)
                elemmul_f64_idx(n, x, y, z, idx);
            else
                elemmul_f64(n, x, y, z);
        }
        break;

    case KERNEL_STENCIL3:
        if (dtype == DTYPE_F32)
            stencil3_f32(n, (float)STENCIL_A, (float)STENCIL_B,
                         (float)STENCIL_C, x, y);
        else
            stencil3_f64(n, STENCIL_A, STENCIL_B, STENCIL_C, x, y);
        break;
    }
}

/* Elementwise relative-tolerance comparison of two dtype-typed arrays. */
bool nearly_equal_arr(const void *a, const void *b, size_t n, dtype_t dtype,
                      double rel_tol) {
    for (size_t i = 0; i < n; i++) {
        double av =
            dtype == DTYPE_F32 ? ((const float *)a)[i] : ((const double *)a)[i];
        double bv =
            dtype == DTYPE_F32 ? ((const float *)b)[i] : ((const double *)b)[i];

        double diff = fabs(av - bv);
        double scale = fmax(fabs(av), fabs(bv));
        if (scale > 0.0) {
            if (diff / scale > rel_tol)
                return 0;
        } else if (diff > rel_tol) {
            return 0;
        }
    }
    return 1;
}

/* Returns idx if non-NULL, otherwise allocates and fills a length-n
 * identity array into *owned (caller must free(*owned); it's left NULL
 * when idx was already non-NULL, so a plain free(NULL) is always safe).
 * Used only inside the one-time correctness check below, to give the
 * (always idx-based) reference implementation something to index with
 * even when the kernel-under-test is the direct, non-idx version. */
const size_t *ensure_idx(const size_t *idx, size_t n, size_t **owned) {
    *owned = NULL;
    if (idx) {
        return idx;
    }

    *owned = malloc(n * sizeof(size_t));

    for (size_t i = 0; i < n; i++) {
        (*owned)[i] = i;
    }

    return *owned;
}

/*
 * Correctness check, run once per process before timing starts. Compares
 * the kernel-under-test -- whichever code path (direct or _idx) is about
 * to be timed -- against the always--O0 reference implementation
 * (src/common/reference.c) on a fresh copy of the same input data.
 *
 * float32 gets a looser tolerance than float64: SIMD reductions/FMA can
 * legitimately reorder floating-point operations relative to the scalar
 * reference, and floating-point addition isn't associative, so small
 * differences there are expected and not a bug.
 */
static int run_correctness_check(kernel_t kernel, dtype_t dtype, size_t n,
                                 const void *x, const void *y,
                                 const size_t *idx) {

    size_t elem_size = (dtype == DTYPE_F32) ? sizeof(float) : sizeof(double);
    double rel_tol = (dtype == DTYPE_F32) ? 1e-3 : 1e-9;
    int ok = 1;
    size_t *owned_idx = NULL;

    const size_t *ref_idx = ensure_idx(idx, n, &owned_idx);

    switch (kernel) {
    case KERNEL_SAXPY: {
        void *y_ref = malloc(n * elem_size);
        void *y_test = malloc(n * elem_size);

        memcpy(y_ref, y, n * elem_size);
        memcpy(y_test, y, n * elem_size);

        if (dtype == DTYPE_F32) {
            saxpy_f32_ref(n, (float)A_COEF, x, y_ref, ref_idx);
            if (idx)
                saxpy_f32_idx(n, (float)A_COEF, x, y_test, idx);
            else
                saxpy_f32(n, (float)A_COEF, x, y_test);
        } else {
            saxpy_f64_ref(n, A_COEF, x, y_ref, ref_idx);
            if (idx)
                saxpy_f64_idx(n, A_COEF, x, y_test, idx);
            else
                saxpy_f64(n, A_COEF, x, y_test);
        }

        ok = nearly_equal_arr(y_ref, y_test, n, dtype, rel_tol);

        free(y_ref);
        free(y_test);
        break;
    }
    case KERNEL_DOTPROD: {
        double s_ref, s_test;
        if (dtype == DTYPE_F32) {
            s_ref = dotprod_f32_ref(n, x, y, ref_idx);
            s_test = idx ? dotprod_f32_idx(n, x, y, idx) : dotprod_f32(n, x, y);
        } else {
            s_ref = dotprod_f64_ref(n, x, y, ref_idx);
            s_test = idx ? dotprod_f64_idx(n, x, y, idx) : dotprod_f64(n, x, y);
        }

        double diff = fabs(s_ref - s_test);
        double scale = fmax(fabs(s_ref), fabs(s_test));

        ok = (scale > 0.0) ? (diff / scale <= rel_tol) : (diff <= rel_tol);

        break;
    }
    case KERNEL_ELEMMUL: {
        void *z_ref = malloc(n * elem_size);
        void *z_test = malloc(n * elem_size);
        if (dtype == DTYPE_F32) {
            elemmul_f32_ref(n, x, y, z_ref, ref_idx);
            if (idx)
                elemmul_f32_idx(n, x, y, z_test, idx);
            else
                elemmul_f32(n, x, y, z_test);
        } else {
            elemmul_f64_ref(n, x, y, z_ref, ref_idx);
            if (idx)
                elemmul_f64_idx(n, x, y, z_test, idx);
            else
                elemmul_f64(n, x, y, z_test);
        }

        ok = nearly_equal_arr(z_ref, z_test, n, dtype, rel_tol);

        free(z_ref);
        free(z_test);
        break;
    }
    case KERNEL_STENCIL3: {
        /* calloc, not malloc: stencil3 only ever writes the interior
         * (indices 1..n-2). The boundary elements y[0] and y[n-1] are
         * intentionally left untouched (see stencil3.c), but
         * nearly_equal_arr() below still compares the full [0,n)
         * range for simplicity -- so both buffers need those
         * untouched elements to start out identical (zero), or the
         * check would be comparing uninitialized heap garbage against
         * uninitialized heap garbage and could fail (or "pass" for
         * the wrong reason) depending on allocator behavior. */
        void *y_ref = calloc(n, elem_size);
        void *y_test = calloc(n, elem_size);
        if (dtype == DTYPE_F32) {
            stencil3_f32_ref(n, (float)STENCIL_A, (float)STENCIL_B,
                             (float)STENCIL_C, x, y_ref);
            stencil3_f32(n, (float)STENCIL_A, (float)STENCIL_B,
                         (float)STENCIL_C, x, y_test);
        } else {
            stencil3_f64_ref(n, STENCIL_A, STENCIL_B, STENCIL_C, x, y_ref);
            stencil3_f64(n, STENCIL_A, STENCIL_B, STENCIL_C, x, y_test);
        }

        ok = nearly_equal_arr(y_ref, y_test, n, dtype, rel_tol);

        free(y_ref);
        free(y_test);
        break;
    }
    }

    free(owned_idx);
    return ok;
}

/* Analytical FLOP/byte counts per kernel -- these are architectural
 * estimates (e.g. stencil3 counts 3 reads + 1 write per interior element,
 * ignoring cache reuse of neighbors), used consistently across all
 * variants so speedup/throughput comparisons stay apples-to-apples. */
static void compute_throughput(kernel_t kernel, dtype_t dtype, size_t n,
                               double time_ns, double *gflops, double *gbps) {
    double elem = (dtype == DTYPE_F32) ? 4.0 : 8.0;
    double flops = 0.0, bytes = 0.0;

    switch (kernel) {
    case KERNEL_SAXPY:
        flops = 2.0 * (double)n;        /* 1 mul + 1 add per element */
        bytes = 3.0 * (double)n * elem; /* read x, read y, write y */
        break;
    case KERNEL_DOTPROD:
        flops = 2.0 * (double)n;        /* 1 mul + 1 add per element */
        bytes = 2.0 * (double)n * elem; /* read x, read y */
        break;
    case KERNEL_ELEMMUL:
        flops = 1.0 * (double)n;        /* 1 mul per element */
        bytes = 3.0 * (double)n * elem; /* read x, read y, write z */
        break;
    case KERNEL_STENCIL3: {
        double m = (n > 2) ? (double)(n - 2) : 0.0;
        flops = 5.0 * m;        /* 3 mul + 2 add per output */
        bytes = 4.0 * m * elem; /* 3 reads + 1 write per output */
        break;
    }
    }

    double seconds = time_ns * 1e-9;
    *gflops = (seconds > 0.0) ? (flops / seconds) / 1e9 : 0.0;
    *gbps =
        (seconds > 0.0) ? (bytes / seconds) / (1024.0 * 1024.0 * 1024.0) : 0.0;
}

static void print_usage(const char *prog) {
    fprintf(
        stderr,
        "usage: %s --kernel {saxpy|dotprod|elemmul|stencil3} --dtype {f32|f64} "
        "--n N\n"
        "          [--stride S] [--align-offset BYTES] [--reps R] [--warmup "
        "W]\n"
        "          [--inner I] [--variant NAME] [--seed SEED]\n"
        "\n"
        "  --stride: 1 = unit stride (default), >1 = strided, <=0 = gather "
        "(random)\n"
        "  --inner:  kernel calls timed together per repetition, to keep each\n"
        "            timed block well above clock_gettime's "
        "resolution/overhead\n"
        "            even for small --n\n",
        prog);
}

int main(int argc, char **argv) {
    const char *kernel_arg = NULL;
    const char *dtype_arg = NULL;
    size_t n = 0;
    size_t align_offset = 0;
    long stride = 1;
    int reps = 11;
    int warmup = 3;
    int inner = 16;
    uint64_t seed = 12345;
    const char *variant = "unknown";

    static struct option long_opts[] = {
        {"kernel", required_argument, 0, 'k'},
        {"dtype", required_argument, 0, 'd'},
        {"n", required_argument, 0, 'n'},
        {"align-offset", required_argument, 0, 'a'},
        {"stride", required_argument, 0, 's'},
        {"reps", required_argument, 0, 'r'},
        {"warmup", required_argument, 0, 'w'},
        {"inner", required_argument, 0, 'i'},
        {"seed", required_argument, 0, 'S'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int c;
    while ((c = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (c) {
        case 'k':
            kernel_arg = optarg;
            break;
        case 'd':
            dtype_arg = optarg;
            break;
        case 'n':
            n = strtoull(optarg, NULL, 10);
            break;
        case 'a':
            align_offset = strtoull(optarg, NULL, 10);
            break;
        case 's':
            stride = strtol(optarg, NULL, 10);
            break;
        case 'r':
            reps = atoi(optarg);
            break;
        case 'w':
            warmup = atoi(optarg);
            break;
        case 'i':
            inner = atoi(optarg);
            break;
        case 'v':
            variant = optarg;
            break;
        case 'S':
            seed = strtoull(optarg, NULL, 10);
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return (c == 'h') ? 0 : 1;
        }
    }

    if (!kernel_arg || !dtype_arg || n == 0) {
        fprintf(stderr, "error: --kernel, --dtype, and --n are required\n");
        print_usage(argv[0]);
        return 1;
    }

    kernel_t kernel = parse_kernel(kernel_arg);
    dtype_t dtype = parse_dtype(dtype_arg);

    if (kernel == KERNEL_STENCIL3 && n < 3) {
        fprintf(stderr, "error: --n must be >= 3 for stencil3\n");
        return 1;
    }
    if (reps < 1 || warmup < 0 || inner < 1) {
        fprintf(stderr, "error: --reps >= 1, --warmup >= 0, --inner >= 1\n");
        return 1;
    }

    rng_t rng;
    rng_seed(&rng, seed);

    size_t elem_size = (dtype == DTYPE_F32) ? sizeof(float) : sizeof(double);

    void *x =
        alloc_aligned_offset(n * elem_size, DEFAULT_ALIGNMENT, align_offset);
    void *y =
        alloc_aligned_offset(n * elem_size, DEFAULT_ALIGNMENT, align_offset);
    void *z = NULL;
    if (kernel == KERNEL_ELEMMUL) {
        z = alloc_aligned_offset(n * elem_size, DEFAULT_ALIGNMENT,
                                 align_offset);
    }

    /* idx stays NULL (and is never allocated) for the unit-stride default
     * (--stride 1) and for stencil3: run_kernel_once() treats a NULL idx
     * as "use the direct, contiguous kernel". It's only built when the
     * stride/gather access pattern is actually being exercised. */
    size_t *idx = NULL;
    if (kernel != KERNEL_STENCIL3 && stride != 1) {
        idx = malloc(n * sizeof(size_t));
        build_index_array(idx, n, stride, &rng);
    }

    if (dtype == DTYPE_F32) {
        fill_random_f32(x, n, &rng);
        fill_random_f32(y, n, &rng);
    } else {
        fill_random_f64(x, n, &rng);
        fill_random_f64(y, n, &rng);
    }

    /* Correctness check happens once, before any timing. saxpy/elemmul/
     * stencil3 have no data-dependent branches, so letting y (or z)
     * continue accumulating across the timed repetitions below doesn't
     * change performance -- only numerical values, which we've already
     * validated here. */
    int checksum_ok = run_correctness_check(kernel, dtype, n, x, y, idx);

    for (int i = 0; i < warmup; i++) {
        run_kernel_once(kernel, dtype, n, x, y, z, idx);
    }

    for (int rep = 0; rep < reps; rep++) {
        double t0 = now_seconds();
        for (int j = 0; j < inner; j++) {
            run_kernel_once(kernel, dtype, n, x, y, z, idx);
        }
        double t1 = now_seconds();

        double time_ns = (t1 - t0) * 1e9 / (double)inner;
        double gflops, gbps;
        compute_throughput(kernel, dtype, n, time_ns, &gflops, &gbps);

        printf("%s,%s,%s,%zu,%zu,%ld,%d,%.3f,%.4f,%.4f,%d\n",
               kernel_name_of(kernel), variant, dtype_name_of(dtype), n,
               align_offset, stride, rep, time_ns, gflops, gbps, checksum_ok);
    }

    return 0;
}
