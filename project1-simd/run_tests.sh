#!/bin/bash

# Scalar baseline vs. auto-vectorized build for each selected kernel across
# working-set sizes. Report speedup and achieved throughput.
# for ((n = 5 ; n <= 30 ; n++));
# do
#     taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     # taskset -c 0 ./build/bin/harness_scalar --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_scalar --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_scalar --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_default --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     # taskset -c 0 ./build/bin/harness_autovec_default --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_default --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_default --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     # taskset -c 0 ./build/bin/harness_autovec_widest --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
# done

# Locality sweep for one kernel crossing cache levels. Annotate where SIMD
# gains compress as the workload becomes memory-bound.
# for ((n = 2*5 ; n <= 2*26 ; n ++));
# do
#     if (( n % 2 == 0 )); then
#         wss=$(( 2 ** (n / 2) ))
#     else
#         wss=$(( 2 ** ((n - 1) / 2) + 2 ** ((n - 3) / 2) ))
#     fi
#
#     taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss
#     taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss --stride 0 #gather
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss --stride 0 # gather
# done

# Alignment and tail-handling study. Quantify prologue/epilogue, unaligned
# load, or masking effects.
for ((n = 0 ; n <= 128 ; n++));
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 12)) --align-offset $n
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 12)) --align-offset $n
done

# for ((n = 0 ; n < 64 ; n++));
# do
#     taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $(((2 ** 12) + n))
# done
