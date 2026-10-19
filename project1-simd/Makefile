# Project 1 -- SIMD Advantage Profiling
#
# Builds three binaries of the *same* source, differing only in the
# optimization/vectorization flags applied to the kernel + harness code:
#
#   bin/harness_scalar            -O2 -fno-tree-vectorize   (true scalar baseline)
#   bin/harness_autovec_default   -O3                       (what a normal build gets)
#   bin/harness_autovec_widest    -O3 -march=native          (best ISA available on this CPU)
#
# The reference implementations (src/common/reference.c) are always
# compiled with a fixed -O0 -fno-tree-vectorize, independent of variant,
# since they only exist as a non-vectorized correctness oracle -- see
# obj/reference.o below.
#
# This project is small enough (a handful of files) that a full rebuild on
# every `make` is fast, so we don't bother with per-variant object-file
# caching: each variant's sources are just compiled directly into its
# binary in one command.
#
# Usage:
#   make            # builds all three variants into build/bin/
#   make scalar     # builds just one variant
#   make clean

CC ?= gcc
CSTD = -std=c23 -D_POSIX_C_SOURCE=200809L -Wall -Wextra
INCLUDES = -Isrc/common -Isrc/kernels
LDLIBS = -lm

# Override any of these from the command line if needed, e.g.:
#   make AUTOVEC_WIDEST_FLAGS="-O3 -march=native -mprefer-vector-width=512"
SCALAR_FLAGS          ?= -D'VARIANT="scalar"' -O2 -fno-tree-vectorize
AUTOVEC_DEFAULT_FLAGS ?= -D'VARIANT="autovec_default"' -O3
AUTOVEC_WIDEST_FLAGS  ?= -D'VARIANT="autovec_widest"' -O3 -march=native

REF_FLAGS = -O0 -fno-tree-vectorize -g

COMMON_SRC  = src/common/timing.c src/common/alloc.c src/common/datagen.c
KERNEL_SRC  = src/kernels/saxpy.c src/kernels/dotprod.c src/kernels/elemmul.c src/kernels/stencil3.c
HARNESS_SRC = src/harness/main.c
REF_SRC     = src/common/reference.c

.PHONY: all clean scalar autovec_default autovec_widest

all: scalar autovec_default autovec_widest

# --- reference object: built once, fixed flags, linked into every variant ---
build/obj/reference.o: $(REF_SRC) src/kernels/kernels.h
	@mkdir -p build/obj
	$(CC) $(CSTD) $(REF_FLAGS) $(INCLUDES) -c $< -o $@

# --- scalar variant ---
scalar: build/bin/harness_scalar
build/bin/harness_scalar: $(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o
	@mkdir -p build/bin
	$(CC) $(CSTD) $(SCALAR_FLAGS) $(INCLUDES) \
		$(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o \
		-o $@ $(LDLIBS)

# --- autovec-default variant ---
autovec_default: build/bin/harness_autovec_default
build/bin/harness_autovec_default: $(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o
	@mkdir -p build/bin
	$(CC) $(CSTD) $(AUTOVEC_DEFAULT_FLAGS) $(INCLUDES) \
		$(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o \
		-o $@ $(LDLIBS)

# --- autovec-widest variant ---
autovec_widest: build/bin/harness_autovec_widest
build/bin/harness_autovec_widest: $(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o
	@mkdir -p build/bin
	$(CC) $(CSTD) $(AUTOVEC_WIDEST_FLAGS) $(INCLUDES) \
		$(COMMON_SRC) $(KERNEL_SRC) $(HARNESS_SRC) build/obj/reference.o \
		-o $@ $(LDLIBS)

clean:
	rm -rf build
