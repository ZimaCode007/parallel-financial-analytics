#include <iostream>
#include <string>
#include <mpi.h>

#include "data_loader.h"
#include "analytics.h"
#include "omp_engine.h"
#include "mpi_engine.h"
#include "performance.h"

/**
 * Entry point for the Parallel Financial Analytics Engine.
 *
 * Usage (OpenMP / sequential):
 *   ./MCP_final <csv_path> [max_rows] [omp_threads]
 *
 * Usage (MPI):
 *   mpirun -np 4 ./MCP_final <csv_path> [max_rows]
 *
 * The program always runs three variants in order and prints a comparison:
 *   1. Sequential baseline
 *   2. OpenMP parallel (shared-memory)
 *   3. MPI parallel (distributed-memory, only when MPI world > 1)
 */
int main(int argc, char* argv[]) {
    /* ── Initialise MPI ──────────────────────────────────────────── */
    MPI_Init(&argc, &argv);

    MpiEngine mpi_engine;

    /* ── Parse command-line arguments (rank 0 prints usage) ──────── */
    if (argc < 2) {
        if (mpi_engine.is_root()) {
            std::cerr << "Usage: " << argv[0]
                      << " <csv_path> [max_rows=0] [omp_threads=0]\n"
                      << "  max_rows=0  → load entire file\n"
                      << "  omp_threads=0 → use OpenMP default\n";
        }
        MPI_Finalize();
        return 1;
    }

    std::string csv_path    = argv[1];
    size_t      max_rows    = (argc >= 3) ? std::stoull(argv[2]) : 0;
    int         omp_threads = (argc >= 4) ? std::stoi(argv[3])   : 0;

    /* ── Load data (only on rank 0; other ranks receive via MPI) ─── */
    std::vector<Transaction> records;
    if (mpi_engine.is_root()) {
        DataLoader loader(csv_path);
        records = loader.load(max_rows);
    }

    /* ── Collect run statistics ──────────────────────────────────── */
    std::vector<RunStats> stats;

    if (mpi_engine.is_root()) {
        /* 1. Sequential baseline */
        {
            Timer t;
            auto result = Analytics::run_sequential(records);
            double ms   = t.elapsed_ms();
            stats.push_back({"Sequential", ms, 1, result});
            std::cout << "\n[Sequential]\n";
            print_analytics_summary(result);
        }

        /* 2. OpenMP parallel */
        {
            OmpEngine omp(omp_threads);
            Timer t;
            auto result = omp.run(records);
            double ms   = t.elapsed_ms();
            stats.push_back({"OpenMP-" + std::to_string(omp.num_threads()),
                             ms, omp.num_threads(), result});
            std::cout << "\n[OpenMP – " << omp.num_threads() << " threads]\n";
            print_analytics_summary(result);
        }
    }

    /* 3. MPI parallel (all ranks participate) */
    {
        // Barrier ensures timing starts after all ranks are ready.
        MPI_Barrier(MPI_COMM_WORLD);

        Timer t;
        auto result = mpi_engine.run(records);
        double ms   = t.elapsed_ms();

        if (mpi_engine.is_root()) {
            stats.push_back({"MPI-" + std::to_string(mpi_engine.world_size()),
                             ms, mpi_engine.world_size(), result});
            std::cout << "\n[MPI – " << mpi_engine.world_size() << " ranks]\n";
            print_analytics_summary(result);
        }
    }

    /* ── Print performance report (rank 0 only) ──────────────────── */
    if (mpi_engine.is_root()) {
        print_performance_report(stats);
    }

    MPI_Finalize();
    return 0;
}
