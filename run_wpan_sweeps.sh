#!/bin/bash

# WPAN HyStart++ Parameter Sweep Script
# Runs all combinations of nodes, flows, pps, and speed
# Appends results to wpan_vary_speed.csv

NS3_CMD="./ns3 run"
WPAN_AREA=200
SIM_TIME=40

# Define sweep ranges
NODES=(20 40 60 80 100)
FLOWS=(10 20 30 40 50)
PPS=(100 200 300 400 500)
SPEED=(5 10 15 20 25)

total_runs=$((${#NODES[@]} * ${#FLOWS[@]} * ${#PPS[@]} * ${#SPEED[@]}))
current_run=0

echo "WPAN Sweep: Total permutations = $total_runs"
echo "WPAN_AREA = $WPAN_AREA, SIM_TIME = $SIM_TIME"
echo ""

for n in "${NODES[@]}"; do
  for f in "${FLOWS[@]}"; do
    for p in "${PPS[@]}"; do
      for s in "${SPEED[@]}"; do
        current_run=$((current_run + 1))
        echo "[$current_run/$total_runs] Running: nodes=$n flows=$f pps=$p speed=$s"
        
        $NS3_CMD "hystart_combined --mode=wpan --singleRun=1 --nodes=$n --flows=$f --pps=$p --speed=$s --wpanArea=$WPAN_AREA --simTime=$SIM_TIME" 2>/dev/null
        
        if [ $? -eq 0 ]; then
          echo "  ✓ Complete"
        else
          echo "  ✗ Failed"
        fi
      done
    done
  done
done

echo ""
echo "===== Sweep complete ====="
echo "Results appended to: wpan_vary_speed.csv"
wc -l wpan_vary_speed.csv
