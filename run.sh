#!/bin/bash
# ═══════════════════════════════════════════════════════════════
#  Parallel Financial Analytics Engine — run script
#  Edit the parameters below, then run:  bash run.sh
# ═══════════════════════════════════════════════════════════════

# ── Parameters (edit as needed) ───────────────────────────────
CSV_PATH="../data/credit_card_transactions.csv"   # Path to the dataset
MAX_ROWS=0                                         # Rows to load (0 = all)
OMP_THREADS=4                                     # OpenMP thread count
MPI_PROCS=2                                       # MPI process count
SAMPLE_RATIO=1                                  # Sample ratio: 1.0 = all data, 0.5 = random 50%
# ──────────────────────────────────────────────────────────────

BUILD_DIR="$(cd "$(dirname "$0")/cmake-build-debug" && pwd)"
BIN="$BUILD_DIR/MCP_final"

if [ ! -f "$BIN" ]; then
    echo "[Error] Binary not found: $BIN"
    echo "        Build first in CLion or run: cmake --build cmake-build-debug"
    exit 1
fi

cd "$BUILD_DIR" || exit 1

echo "════════════════════════════════════════════════════════"
echo "  CSV:          $CSV_PATH"
echo "  Max Rows:     $MAX_ROWS (0=all)"
echo "  OpenMP:       $OMP_THREADS threads"
echo "  MPI:          $MPI_PROCS processes"
echo "  Sample Ratio: $SAMPLE_RATIO (1.0=all, 0.5=random 50%)"
echo "════════════════════════════════════════════════════════"
echo ""

echo "▶ [1/3] Running Sequential..."
./MCP_final "$CSV_PATH" seq "$MAX_ROWS" 0 "$SAMPLE_RATIO"
echo ""

echo "▶ [2/3] Running OpenMP ($OMP_THREADS threads)..."
./MCP_final "$CSV_PATH" omp "$MAX_ROWS" "$OMP_THREADS" "$SAMPLE_RATIO"
echo ""

echo "▶ [3/3] Running MPI ($MPI_PROCS processes)..."
mpirun -np "$MPI_PROCS" ./MCP_final "$CSV_PATH" mpi "$MAX_ROWS" 0 "$SAMPLE_RATIO"
echo ""

echo "▶ [4/4] Generating report..."
./MCP_final report
echo ""
echo "✅ Done! Open report.html in browser to view results."
