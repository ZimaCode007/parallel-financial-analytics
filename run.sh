#!/bin/bash
# ═══════════════════════════════════════════════════════════════
#  Parallel Financial Analytics Engine — 运行脚本
#  修改下方参数后，在终端执行：  bash run.sh
# ═══════════════════════════════════════════════════════════════

# ── 参数配置（按需修改） ──────────────────────────────────────
CSV_PATH="../data/credit_card_transactions.csv"   # 数据集路径
MAX_ROWS=0                                         # 加载行数（0=全部）
OMP_THREADS=4                                     # OpenMP 线程数
MPI_PROCS=2                                       # MPI 进程数
SAMPLE_RATIO=1                                  # 采样比例：1.0=全部数据，0.5=随机50%
# ──────────────────────────────────────────────────────────────

BUILD_DIR="$(cd "$(dirname "$0")/cmake-build-debug" && pwd)"
BIN="$BUILD_DIR/MCP_final"

if [ ! -f "$BIN" ]; then
    echo "[Error] 可执行文件不存在: $BIN"
    echo "        请先在 CLion 中 Build 或执行: cmake --build cmake-build-debug"
    exit 1
fi

cd "$BUILD_DIR" || exit 1

echo "════════════════════════════════════════════════════════"
echo "  CSV:          $CSV_PATH"
echo "  Max Rows:     $MAX_ROWS (0=全部)"
echo "  OpenMP:       $OMP_THREADS 线程"
echo "  MPI:          $MPI_PROCS 进程"
echo "  Sample Ratio: $SAMPLE_RATIO (1.0=全部, 0.5=随机50%)"
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
