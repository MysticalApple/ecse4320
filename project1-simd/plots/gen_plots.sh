#!/bin/bash

./plot_time.py ../data/speedup.csv &
./plot_flops.py ../data/speedup.csv &
./plot_gbps.py ../data/speedup.csv &
./plot_speedup.py ../data/speedup.csv &

./plot_locality.py ../data/locality.csv &

./plot_alignment.py ../data/alignment.csv &
./plot_tail.py ../data/tail.csv &

./plot_stride.py ../data/stride.csv &

./plot_datatype.py ../data/datatype.csv &

wait
