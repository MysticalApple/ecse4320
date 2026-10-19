#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

/* Cache-line / max-vector-width alignment used throughout the harness. */
#define DEFAULT_ALIGNMENT 64

/*
 * Allocates at least `nbytes` usable bytes, aligned to `alignment`, with
 * the returned pointer shifted `offset_bytes` from that alignment boundary
 * -- this is how the alignment/tail experiment deliberately misaligns an
 * array's starting address.
 *
 * Exits the process on allocation failure.
 *
 * Not intended to be freed: this harness is a short-lived, single-purpose
 * benchmark process, and every allocation lives for the process's whole
 * run. The OS reclaims everything on exit, so we don't bother tracking the
 * original (pre-offset) base pointer just to free it later.
 */
void *alloc_aligned_offset(size_t nbytes, size_t alignment, size_t offset_bytes);

#endif /* ALLOC_H */
