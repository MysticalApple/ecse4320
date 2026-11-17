#!/bin/bash

rm -f -i data/*

exec 3>&1

# Scalar baseline vs. auto-vectorized build for each selected kernel across
# working-set sizes. Report speedup and achieved throughput.
echo "Running speedup and throughput tests..." >&3
exec > data/speedup.csv
for ((n = 5 ; n <= 30 ; n++));
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    # taskset -c 0 ./build/bin/harness_scalar --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_scalar --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_scalar --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    # taskset -c 0 ./build/bin/harness_autovec_default --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    # taskset -c 0 ./build/bin/harness_autovec_widest --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel stencil3 --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
done

# Locality sweep for one kernel crossing cache levels. Annotate where SIMD
# gains compress as the workload becomes memory-bound.
echo "Running locality sweep..." >&3
exec > data/locality.csv
for ((n = 2*5 ; n <= 2*26 ; n ++));
do
    if (( n % 2 == 0 )); then
        wss=$(( 2 ** (n / 2) ))
    else
        wss=$(( 2 ** ((n - 1) / 2) + 2 ** ((n - 3) / 2) ))
    fi

    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss --stride 0 #gather
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $wss --stride 0 # gather
done

# Alignment and tail-handling study. Quantify prologue/epilogue, unaligned
# load, or masking effects.
echo "Running alignment sweep..." >&3
exec > data/alignment.csv
for ((n = 0 ; n <= 128 ; n++));
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 15)) --align-offset $n
    taskset -c 0 ./build/bin/harness_autovec_default --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 15)) --align-offset $n
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 15)) --align-offset $n
done


echo "Running tail sweep..." >&3
exec > data/tail.csv
for ((n = 0 ; n <= 128 ; n++));
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $(((2 ** 15) + n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $(((2 ** 15) + n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $(((2 ** 15) + n))
done

# Stride/gather study. Show how non-unit stride reduces effective bandwidth and
# SIMD efficiency.
echo "Running stride tests..." >&3
exec > data/stride.csv
for n in 1 2 4 8 0
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 23)) --stride $n
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** 23)) --stride $n
done

# Data-type comparison. Explain results using lane count, arithmetic intensity,
# and operation cost.
echo "Running data-type comparisons..." >&3
exec > data/datatype.csv
for k in "saxpy" "elemmul" "stencil3"
do
    taskset -c 0 ./build/bin/harness_scalar --kernel $k --dtype f32 --inner 10 --reps 10 --n $((2 ** 15))
    taskset -c 0 ./build/bin/harness_scalar --kernel $k --dtype f64 --inner 10 --reps 10 --n $((2 ** 14))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel $k --dtype f32 --inner 10 --reps 10 --n $((2 ** 15))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel $k --dtype f64 --inner 10 --reps 10 --n $((2 ** 14))
done

# Roofline interpretation for at least one kernel using measured memory
# bandwidth and estimated CPU peak FLOP rate.
echo "Running tests for roofline data..." >&3
exec > data/roofline.csv
for n in 1 2 4 8
do
    for k in "saxpy" "elemmul" "stencil3" "dotprod"
    do
        taskset -c 0 ./build/bin/harness_scalar --kernel $k --dtype f32 --inner 10 --reps 10 --n $((2 ** 24))
        taskset -c 0 ./build/bin/harness_scalar --kernel $k --dtype f64 --inner 10 --reps 10 --n $((2 ** 24))
        taskset -c 0 ./build/bin/harness_autovec_widest --kernel $k --dtype f32 --inner 10 --reps 10 --n $((2 ** 24))
        taskset -c 0 ./build/bin/harness_autovec_widest --kernel $k --dtype f64 --inner 10 --reps 10 --n $((2 ** 24))
    done
done

echo "Done." >&3
