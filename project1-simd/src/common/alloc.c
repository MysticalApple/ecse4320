#include "alloc.h"
#include <stdio.h>
#include <stdlib.h>

void *alloc_aligned_offset(size_t nbytes, size_t alignment, size_t offset_bytes) {
    /* Extra headroom so shifting the returned pointer by offset_bytes still
     * leaves nbytes of valid, addressable space past it. */
    size_t total = nbytes + offset_bytes + alignment;

    void *base = NULL;
    int rc = posix_memalign(&base, alignment, total);
    if (rc != 0 || base == NULL) {
        fprintf(stderr, "alloc_aligned_offset: posix_memalign(%zu bytes, align %zu) failed\n",
                total, alignment);
        exit(1);
    }

    return (char *)base + offset_bytes;
}
