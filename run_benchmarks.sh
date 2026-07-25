#!/usr/bin/env bash
# Benchmark runner for sequential, OpenMP, MPI, and Hybrid configurations.
# Results are written to benchmark_results/<timestamp>/performance.csv.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/cmake-build-debug}"
BIN="$BUILD_DIR/MCP_final"
CSV_PATH="${CSV_PATH:-$SCRIPT_DIR/data/credit_card_transactions.csv}"
MAX_ROWS="${MAX_ROWS:-0}"
SAMPLE_RATIO="${SAMPLE_RATIO:-1.0}"
OUTPUT_ROOT="${OUTPUT_ROOT:-$SCRIPT_DIR/benchmark_results}"

if [[ ! -x "$BIN" ]]; then
    echo "[Error] Executable not found: $BIN"
    echo "        Build it first with: cmake --build $BUILD_DIR"
    exit 1
fi

if [[ ! -f "$CSV_PATH" ]]; then
    echo "[Error] Dataset not found: $CSV_PATH"
    exit 1
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUTPUT_DIR="$OUTPUT_ROOT/$RUN_ID"
CSV_FILE="$OUTPUT_DIR/performance.csv"
LOG_FILE="$OUTPUT_DIR/benchmark.log"
RESULTS_DIR="$BUILD_DIR/results"
mkdir -p "$OUTPUT_DIR"

cat > "$CSV_FILE" <<EOF
run_type,configuration,mpi_processes,omp_threads,elapsed_ms,speedup
EOF

cat > "$OUTPUT_DIR/metadata.txt" <<EOF
dataset=$CSV_PATH
max_rows=$MAX_ROWS
sample_ratio=$SAMPLE_RATIO
created_at=$(date '+%Y-%m-%d %H:%M:%S %Z')
EOF

run_command() {
    local mode="$1"
    local mpi_processes="$2"
    local omp_threads="$3"

    if [[ "$mpi_processes" -eq 1 ]]; then
        "$BIN" "$CSV_PATH" "$mode" "$MAX_ROWS" "$omp_threads" "$SAMPLE_RATIO" >> "$LOG_FILE" 2>&1
    else
        mpirun -np "$mpi_processes" "$BIN" "$CSV_PATH" "$mode" "$MAX_ROWS" "$omp_threads" "$SAMPLE_RATIO" >> "$LOG_FILE" 2>&1
    fi
}

elapsed_from_result() {
    local result_file="$1"
    local elapsed
    elapsed="$(awk -F= '$1 == "elapsed_ms" { print $2; exit }' "$result_file")"
    if [[ -z "$elapsed" ]]; then
        echo "[Error] Could not read elapsed_ms from $result_file" >&2
        exit 1
    fi
    printf '%s' "$elapsed"
}

record_run() {
    local run_type="$1"
    local configuration="$2"
    local mpi_processes="$3"
    local omp_threads="$4"
    local result_name="$5"
    local baseline_ms="$6"
    local elapsed_ms speedup

    elapsed_ms="$(elapsed_from_result "$RESULTS_DIR/$result_name.result")"
    speedup="$(awk -v baseline="$baseline_ms" -v elapsed="$elapsed_ms" 'BEGIN {
        if (elapsed <= 0) exit 1;
        printf "%.6f", baseline / elapsed;
    }')"
    printf '%s,%s,%s,%s,%s,%s\n' \
        "$run_type" "$configuration" "$mpi_processes" "$omp_threads" "$elapsed_ms" "$speedup" >> "$CSV_FILE"
    echo "[Done] $configuration: ${elapsed_ms} ms, speedup ${speedup}x"
}

cd "$BUILD_DIR"
echo "Benchmark output: $OUTPUT_DIR"
echo "Logs: $LOG_FILE"

echo "[1/9] Running sequential baseline..."
run_command seq 1 0
BASELINE_MS="$(elapsed_from_result "$RESULTS_DIR/seq.result")"
printf 'Sequential,Sequential,1,1,%s,1.000000\n' "$BASELINE_MS" >> "$CSV_FILE"
echo "[Done] Sequential baseline: ${BASELINE_MS} ms"

for threads in 2 4 8; do
    echo "[OpenMP] Running with $threads threads..."
    run_command omp 1 "$threads"
    record_run "OpenMP" "OpenMP-$threads" 1 "$threads" "omp" "$BASELINE_MS"
done

for processes in 2 4 8; do
    echo "[MPI] Running with $processes processes..."
    run_command mpi "$processes" 0
    record_run "MPI" "MPI-$processes" "$processes" 1 "mpi" "$BASELINE_MS"
done

for omp_threads in 2 4; do
    echo "[Hybrid] Running with 2 MPI processes x $omp_threads OpenMP threads..."
    run_command hybrid 2 "$omp_threads"
    record_run "Hybrid" "MPI-2xOMP-$omp_threads" 2 "$omp_threads" "hybrid" "$BASELINE_MS"
done

echo ""
echo "Benchmark complete. CSV data: $CSV_FILE"
echo "No HTML report was generated."
