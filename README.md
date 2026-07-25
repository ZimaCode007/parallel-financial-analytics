# Parallel Financial Analytics Engine

A high-performance financial transaction analytics engine implemented in C++, leveraging **OpenMP** (shared-memory parallelism) and **MPI** (distributed-memory parallelism) to process large-scale datasets efficiently.

**Authors:** Nguyen Phu Pham · Roshika Pant · Bingqian Yang

---

## Quick Start: Scripts

Build the project once before using either script:

```bash
cmake --build cmake-build-debug
```

### `run.sh` — Run the full analytics workflow

Edit the configuration block at the top of `run.sh` to select the dataset,
row limit, sampling ratio, and parallelism settings.  The script runs the
sequential, OpenMP, MPI, and Hybrid modes, then generates a combined HTML
report.

```bash
./run.sh
```

The report is written to `cmake-build-debug/report.html`.

### `run_benchmarks.sh` — Collect benchmark data for charts

This script does not generate an HTML report.  It runs one sequential
baseline, OpenMP and MPI with 2/4/8 workers, plus Hybrid configurations
`MPI-2 × OMP-2` and `MPI-2 × OMP-4`.  It calculates speedup for every run and
writes CSV data and logs to `benchmark_results/<timestamp>/`.

```bash
./run_benchmarks.sh
```

For a quick small-data validation:

```bash
MAX_ROWS=10000 ./run_benchmarks.sh
```

---

## Overview

Modern financial institutions generate millions of transaction records daily. Sequential processing becomes a bottleneck at scale. This project implements a parallel analytics engine that:

- Loads and parses the **IBM Credit Card Transactions Dataset** (CSV format)
- Partitions data for parallel execution across threads or processes
- Computes financial aggregates: SUM, AVG, MAX, COUNT, and GROUP BY
- Benchmarks sequential vs. OpenMP vs. MPI execution and reports speedup and efficiency

---

## Architecture

```
MCP_final/
├── main.cpp                    # Entry point — orchestrates sequential, OpenMP, MPI, and hybrid modes
├── CMakeLists.txt              # Build configuration (OpenMP + MPI)
├── data/                       # Place IBM dataset CSV files here (not tracked by git)
├── include/
│   ├── transaction.h           # Transaction record struct
│   ├── data_loader.h           # CSV parsing interface
│   ├── data_partitioner.h      # Data chunking interface
│   ├── analytics.h             # Aggregation functions (SUM/AVG/MAX/COUNT/GroupBy)
│   ├── omp_engine.h            # OpenMP parallel engine interface
│   ├── mpi_engine.h            # MPI distributed engine interface
│   ├── hybrid_engine.h         # Two-level MPI + OpenMP engine interface
│   └── performance.h           # Timer and performance report interface
└── src/
    ├── data_loader.cpp         # CSV parser with quoted-field support
    ├── data_partitioner.cpp    # Index-range and data-copy partitioning
    ├── analytics.cpp           # Pure aggregation implementations
    ├── omp_engine.cpp          # OpenMP parallel execution
    ├── mpi_engine.cpp          # MPI scatter/gather execution
    ├── hybrid_engine.cpp       # MPI distribution plus local OpenMP execution
    └── performance.cpp         # Speedup/efficiency table printer
```

### Module Descriptions

| Module | Responsibility |
|---|---|
| **Data Loader** | Reads CSV line-by-line, handles quoted fields, converts dollar amounts to integer cents |
| **Data Partitioner** | Splits the dataset into equal-sized contiguous chunks by index range (OpenMP) or copied vectors (MPI) |
| **Analytics** | Stateless functions operating on raw pointer ranges — composable with all engines |
| **OpenMP Engine** | Assigns index ranges to threads; uses `#pragma omp critical` for scalar reduction and per-thread maps for group-by |
| **MPI Engine** | Rank 0 scatters `PackedTx` structs via `MPI_Scatterv`; all ranks compute locally; rank 0 gathers via `MPI_Gatherv` with custom map serialisation |
| **Hybrid Engine** | MPI distributes records between processes; each process invokes the OpenMP engine for its local partition; rank 0 merges all partial results |
| **Performance** | Wall-clock `Timer` (nanosecond resolution), `RunStats` record, formatted table with Speedup S(p) = T_seq/T_par and Efficiency E(p) = S(p)/p |

---

## Dataset

This project uses the [IBM Credit Card Transactions Dataset](https://www.kaggle.com/datasets/ealtman2019/ibm-transactions-for-anti-money-laundering-aml).

Download the CSV file and place it in the `data/` directory:

```
data/
└── transactions.csv
```

Expected CSV columns (0-indexed):

| Index | Field |
|---|---|
| 0 | Transaction ID |
| 1 | Date (YYYY-MM-DD) |
| 2 | Amount (e.g. `$12.34`) |
| 3 | Merchant Name |
| 4 | Merchant Category |
| 5 | State |
| 6 | Card Last 4 Digits |

---

## Requirements

- CMake ≥ 3.18
- C++20 compiler (GCC 10+ or Clang 12+)
- OpenMP (bundled with most compilers)
- MPI implementation: [OpenMPI](https://www.open-mpi.org/) or [MPICH](https://www.mpich.org/)

### macOS

```bash
brew install open-mpi
```

### Ubuntu / Debian

```bash
sudo apt install libopenmpi-dev openmpi-bin
```

---

## Build

```bash
# Configure (Release for benchmarking, Debug for development)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build -j$(nproc)
```

The binary is produced at `build/MCP_final`.

---

## Usage

```
./MCP_final <csv_path> <seq|omp|mpi|hybrid|all> [max_rows] [omp_threads] [sample_ratio]
./MCP_final report
```

| Argument | Default | Description |
|---|---|---|
| `csv_path` | required | Path to the transactions CSV file (`report` is a standalone mode) |
| `mode` | required | `seq`, `omp`, `mpi`, `hybrid`, or `all` |
| `max_rows` | `0` (all) | Limit number of rows loaded (useful for testing) |
| `omp_threads` | `0` (auto) | Number of OpenMP threads |

### Run all modes

```bash
# Single node — runs sequential, OpenMP, MPI, and hybrid (with 1 MPI rank)
./build/MCP_final data/transactions.csv all

# MPI with 4 processes (+ sequential and OpenMP on rank 0)
mpirun -np 4 ./build/MCP_final data/transactions.csv all

# Limit to 500 000 rows, use 8 OpenMP threads, 4 MPI ranks
mpirun -np 4 ./build/MCP_final data/transactions.csv all 500000 8
```

### Run hybrid MPI + OpenMP only

MPI partitions the records between processes, and each process uses the given
number of OpenMP threads for its partition.

```bash
# 4 MPI processes, each with 8 OpenMP threads
mpirun -np 4 ./build/MCP_final data/transactions.csv hybrid 500000 8
```

---

## Sample Output

```
[DataLoader] Loaded 5000000 transactions from data/transactions.csv

[Sequential]
--- Analytics Summary ---
  Record count : 5000000
  Total amount : $312,847,502.15
  Average      : $62.56
  Maximum      : $9,999.99
  Top 5 by category:
    Grocery Stores              $48,201,034.00
    ...
  Top 5 by state:
    CA                          $31,045,200.00
    ...

[OpenMP – 8 threads]
--- Analytics Summary ---
  (same results as sequential)

[MPI – 4 ranks]
--- Analytics Summary ---
  (same results as sequential)

=================================================================
  Parallel Financial Analytics Engine — Performance Report
=================================================================
Run                Time (ms)   Speedup     Efficiency
-----------------------------------------------------------------
Sequential          8423.14        1.00        1.00
OpenMP-8            1204.67        6.99        0.87
MPI-4               2318.45        3.63        0.91
=================================================================
```

---

## Design Notes

- **Integer cents:** All monetary values are stored as `long long` cents internally to avoid floating-point accumulation errors during parallel reduction over millions of rows.
- **Zero-copy OpenMP:** The OpenMP engine operates on index ranges into the shared vector — no data is copied between threads.
- **MPI serialisation:** Group-by `unordered_map` results are packed into a compact wire format (`key_len | key bytes | value`) and transferred with `MPI_Gatherv`, avoiding any custom MPI datatype registration.
- **Composable analytics:** All aggregation functions accept raw pointer ranges `[begin, end)` and are reused across the sequential baseline and both parallel engines.

---

## Performance Metrics

| Metric | Formula |
|---|---|
| Speedup | S(p) = T_sequential / T_parallel |
| Efficiency | E(p) = S(p) / p |

Experiments vary dataset size and thread/process count to analyse scalability across both parallelism models.

---

## License

For academic use only (course project). Dataset terms follow the IBM/Kaggle dataset license.

---

## AI Assistance Declaration

The overall module design, system architecture, and function interfaces were
designed by the project team. AI tools were used as assistance for parts of the
internal implementation of selected functions and for generating the HTML
reporting component. All AI-assisted output was reviewed, integrated, and
validated by the project team.
