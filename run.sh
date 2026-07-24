#!/bin/bash
# ═══════════════════════════════════════════════════════════════
#  Parallel Financial Analytics Engine — Run Script
#  Edit the parameters below, then run: bash run.sh
# ═══════════════════════════════════════════════════════════════

# ── Configuration (edit as needed) ────────────────────────────
CSV_PATH="../data/credit_card_transactions.csv"   # Dataset path
MAX_ROWS=0                                         # Rows to load (0 = all)
OMP_THREADS=8                                      # OpenMP thread count
MPI_PROCS=8                                        # MPI process count
HYBRID_MPI_PROCS=2                                 # Hybrid MPI process count
HYBRID_OMP_THREADS=4                               # OpenMP threads per Hybrid MPI process
SAMPLE_RATIO=1                                     # Sampling ratio: 1.0 = all, 0.5 = random 50%
# ──────────────────────────────────────────────────────────────

BUILD_DIR="$(cd "$(dirname "$0")/cmake-build-debug" && pwd)"
BIN="$BUILD_DIR/MCP_final"

if [ ! -f "$BIN" ]; then
    echo "[Error] Executable not found: $BIN"
    echo "        Build it in CLion or run: cmake --build cmake-build-debug"
    exit 1
fi

cd "$BUILD_DIR" || exit 1

echo "════════════════════════════════════════════════════════"
echo "  CSV:          $CSV_PATH"
echo "  Max Rows:     $MAX_ROWS (0 = all)"
echo "  OpenMP:       $OMP_THREADS threads"
echo "  MPI:          $MPI_PROCS processes"
echo "  Hybrid:       $HYBRID_MPI_PROCS MPI processes × $HYBRID_OMP_THREADS OpenMP threads/process"
echo "  Sample Ratio: $SAMPLE_RATIO (1.0 = all, 0.5 = random 50%)"
echo "════════════════════════════════════════════════════════"
echo ""

echo "▶ [1/5] Running Sequential..."
./MCP_final "$CSV_PATH" seq "$MAX_ROWS" 0 "$SAMPLE_RATIO"
echo ""

echo "▶ [2/5] Running OpenMP ($OMP_THREADS threads)..."
./MCP_final "$CSV_PATH" omp "$MAX_ROWS" "$OMP_THREADS" "$SAMPLE_RATIO"
echo ""

echo "▶ [3/5] Running MPI ($MPI_PROCS processes)..."
mpirun -np "$MPI_PROCS" ./MCP_final "$CSV_PATH" mpi "$MAX_ROWS" 0 "$SAMPLE_RATIO"
echo ""

echo "▶ [4/5] Running Hybrid ($HYBRID_MPI_PROCS MPI processes × $HYBRID_OMP_THREADS OpenMP threads)..."
mpirun -np "$HYBRID_MPI_PROCS" ./MCP_final "$CSV_PATH" hybrid "$MAX_ROWS" "$HYBRID_OMP_THREADS" "$SAMPLE_RATIO"
echo ""

echo "▶ [5/5] Generating report..."
./MCP_final report
echo ""
echo "✅ Done! Open $BUILD_DIR/report.html in browser to view results."
