#!/bin/bash

./plot_baseline.py &
./plot_flops.py &
./plot_gbps.py &

wait
