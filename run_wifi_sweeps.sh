#!/bin/bash

set -euo pipefail

# WiFi HyStart++ Parameter Sweep Script
# Runs all combinations sequentially so it does not overwhelm the laptop.
# The script resumes from already-recorded rows in wifi_vary_nodes.csv.

NS3_CMD="./ns3 run"
SIM_TIME=40
CSV_FILE="wifi_vary_nodes.csv"
COVERAGE=1

# Define sweep ranges
NODES=(100)
FLOWS=(10 20 30 40 50)
PPS=(100 200 300 400 500)
TX_RANGES=(2 3 4 5)

EXPECTED_PER_NODE=$((${#FLOWS[@]} * ${#PPS[@]} * ${#TX_RANGES[@]}))
TOTAL_RUNS=$((${#NODES[@]} * EXPECTED_PER_NODE))

combo_exists() {
  local node="$1"
  local flows="$2"
  local pps="$3"
  local txr="$4"

  if [ ! -f "$CSV_FILE" ]; then
    return 1
  fi

  awk -F',' -v n="$node" -v f="$flows" -v p="$pps" -v t="$txr" -v c="$COVERAGE" '
    NR > 1 && $1 == n && $2 == f && $3 == p && $5 == t && $6 == c { found = 1; exit }
    END { exit(found ? 0 : 1) }
  ' "$CSV_FILE"
}

echo "WiFi Sweep: Total permutations = $TOTAL_RUNS"
echo "TX_RANGE sweep = ${TX_RANGES[*]}, COVERAGE = $COVERAGE, SIM_TIME = $SIM_TIME"
echo "Sequential mode: on"
echo ""

for n in "${NODES[@]}"; do
  completed=0
  for f in "${FLOWS[@]}"; do
    for p in "${PPS[@]}"; do
      for txr in "${TX_RANGES[@]}"; do
        completed=$((completed + 1))

        if combo_exists "$n" "$f" "$p" "$txr"; then
          continue
        fi

        echo "[node=$n $completed/$EXPECTED_PER_NODE] flows=$f pps=$p txRange=$txr"
        $NS3_CMD "hystart_combined --mode=wifi --singleRun=1 --nodes=$n --flows=$f --pps=$p --coverage=$COVERAGE --txRange=$txr --simTime=$SIM_TIME --wifiCsv=$CSV_FILE"
      done
    done
  done
done

echo ""
echo "===== Sweep complete ====="
echo "Results written to: $CSV_FILE"
wc -l "$CSV_FILE"
