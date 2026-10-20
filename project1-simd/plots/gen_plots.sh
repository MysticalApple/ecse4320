#!/bin/bash

./plot_baseline.py data.csv &
./plot_flops.py data.csv &
./plot_gbps.py data.csv &
./plot_speedup.py data.csv &

./plot_locality_sweep.py locality_sweep.csv &

wait
