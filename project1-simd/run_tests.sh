#!/bin/bash

# Scalar baseline vs. auto-vectorized build for each selected kernel across
# working-set sizes. Report speedup and achieved throughput.
for ((n = 5 ; n <= 31 ; n++));
do
    taskset -c 0 ./build/bin/harness_scalar --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_scalar --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_scalar --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_default --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel saxpy --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel dotprod --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
    taskset -c 0 ./build/bin/harness_autovec_widest --kernel elemmul --dtype f32 --inner 10 --reps 10 --n $((2 ** $n))
done
