#include <iostream>
#include <string>
#include <filesystem>
#include <mpi.h>

#include "data_loader.h"
#include "analytics.h"
#include "omp_engine.h"
#include "mpi_engine.h"
#include "hybrid_engine.h"
#include "performance.h"
#include "report_generator.h"
#include "result_io.h"

/**
 * Entry point: Parallel Financial Analytics Engine
 *
 * Two execution modes are supported:
 *
 *   Separate-run mode (recommended; engines run independently without contention):
 *     ./MCP_final <csv_path> seq   [max_rows]                 # Sequential only
 *     ./MCP_final <csv_path> omp   [max_rows] [omp_threads]   # OpenMP only
 *     mpirun -np 4 ./MCP_final <csv_path> mpi [max_rows]      # MPI only
 *     mpirun -np 4 ./MCP_final <csv_path> hybrid [max_rows] [omp_threads] # MPI + OpenMP
 *     ./MCP_final report                                       # Merge results and generate report
 *
 *   Combined mode (all engines in one invocation; backward-compatible):
 *     ./MCP_final <csv_path> all   [max_rows] [omp_threads]
 *     mpirun -np 4 ./MCP_final <csv_path> all [max_rows] [omp_threads]
 *
 *   Each separate-run mode saves results to a .result file under results/.
 *   The report mode reads those files and produces a combined performance
 *   comparison report and report.html.
 */

static void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <csv_path> seq   [max_rows] [threads] [sample_ratio]\n"
              << "  " << prog << " <csv_path> omp   [max_rows] [threads] [sample_ratio]\n"
              << "  mpirun -np N " << prog << " <csv_path> mpi [max_rows] [threads] [sample_ratio]\n"
              << "  mpirun -np N " << prog << " <csv_path> hybrid [max_rows] [threads] [sample_ratio]\n"
              << "  " << prog << " <csv_path> all   [max_rows] [threads] [sample_ratio]\n"
              << "  " << prog << " report\n"
              << "\n"
              << "  sample_ratio: 0.0~1.0, e.g. 0.5 = random 50% (default 1.0 = use all)\n";
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int mpi_rank = 0, mpi_size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (argc < 2) {
        if (mpi_rank == 0) print_usage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    std::string arg1 = argv[1];

    /* ── report mode: merge existing result files and generate report ── */
    if (arg1 == "report") {
        if (mpi_rank == 0) {
            std::vector<RunStats> stats;
            std::string results_dir = "results";
            // Load in fixed order: seq → omp → mpi → hybrid.
            std::string files[] = {
                results_dir + "/seq.result",
                results_dir + "/omp.result",
                results_dir + "/mpi.result",
                results_dir + "/hybrid.result"
            };
            for (const auto& f : files) {
                if (std::filesystem::exists(f)) {
                    stats.push_back(load_run_stats(f));
                    std::cout << "[Report] Loaded " << f << "\n";
                }
            }
            if (stats.empty()) {
                std::cerr << "[Report] No result files found in results/ directory.\n";
                MPI_Finalize();
                return 1;
            }
            print_performance_report(stats);
            // The original csv_path is not stored in seq.result; use a placeholder.
            generate_html_report("report.html", stats, "(merged from separate runs)");
        }
        MPI_Finalize();
        return 0;
    }

    /* ── All other modes require csv_path and mode ───────────────── */
    if (argc < 3) {
        if (mpi_rank == 0) print_usage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    std::string csv_path    = argv[1];
    std::string mode        = argv[2];
    size_t max_rows         = (argc >= 4) ? std::stoull(argv[3]) : 0;
    int omp_threads         = (argc >= 5) ? std::stoi(argv[4])   : 0;
    double sample_ratio     = (argc >= 6) ? std::stod(argv[5])   : 1.0;

    // Create results directory if it does not exist.
    if (mpi_rank == 0) {
        std::filesystem::create_directories("results");
    }

    bool run_seq = (mode == "seq" || mode == "all");
    bool run_omp = (mode == "omp" || mode == "all");
    bool run_mpi = (mode == "mpi" || mode == "all");
    bool run_hybrid = (mode == "hybrid" || mode == "all");

    /* ── Load data on rank 0 (with optional random sampling) ────── */
    std::vector<Transaction> records;
    if (mpi_rank == 0) {
        DataLoader loader(csv_path);
        records = loader.load(max_rows);

        if (sample_ratio > 0.0 && sample_ratio < 1.0) {
            records = DataLoader::sample(records, sample_ratio);
        }
    }

    std::vector<RunStats> stats;

    /* ── Sequential ───────────────────────────────────────────────── */
    if (run_seq && mpi_rank == 0) {
        Timer t;
        auto result = Analytics::run_sequential(records);
        double ms   = t.elapsed_ms();

        RunStats s = {"Sequential", ms, 1, result};
        stats.push_back(s);
        save_run_stats("results/seq.result", s);

        std::cout << "\n[Sequential]\n";
        print_analytics_summary(result);
    }

    /* ── OpenMP ───────────────────────────────────────────────────── */
    if (run_omp && mpi_rank == 0) {
        OmpEngine omp(omp_threads);
        Timer t;
        auto result = omp.run(records);
        double ms   = t.elapsed_ms();

        RunStats s = {"OpenMP-" + std::to_string(omp.num_threads()),
                      ms, omp.num_threads(), result};
        stats.push_back(s);
        save_run_stats("results/omp.result", s);

        std::cout << "\n[OpenMP – " << omp.num_threads() << " threads]\n";
        print_analytics_summary(result);
    }

    /* ── MPI ──────────────────────────────────────────────────────── */
    if (run_mpi) {
        MpiEngine mpi_engine;

        MPI_Barrier(MPI_COMM_WORLD);

        Timer t;
        auto result = mpi_engine.run(records);
        double ms   = t.elapsed_ms();

        if (mpi_rank == 0) {
            RunStats s = {"MPI-" + std::to_string(mpi_engine.world_size()),
                          ms, mpi_engine.world_size(), result};
            stats.push_back(s);
            save_run_stats("results/mpi.result", s);

            std::cout << "\n[MPI – " << mpi_engine.world_size() << " ranks]\n";
            print_analytics_summary(result);
        }
    }

    /* ── Hybrid MPI + OpenMP ─────────────────────────────────────── */
    if (run_hybrid) {
        HybridEngine hybrid(omp_threads);
        MPI_Barrier(MPI_COMM_WORLD);

        Timer t;
        auto result = hybrid.run(records);
        double ms = t.elapsed_ms();

        if (mpi_rank == 0) {
            const int total_workers = hybrid.world_size() * hybrid.omp_threads();
            RunStats s = {"Hybrid-MPI-" + std::to_string(hybrid.world_size()) +
                              "xOMP-" + std::to_string(hybrid.omp_threads()),
                          ms, total_workers, result};
            stats.push_back(s);
            save_run_stats("results/hybrid.result", s);

            std::cout << "\n[Hybrid – " << hybrid.world_size() << " MPI ranks × "
                      << hybrid.omp_threads() << " OpenMP threads]\n";
            print_analytics_summary(result);
        }
    }

    /* ── Output report ───────────────────────────────────────────── */
    if (mpi_rank == 0 && !stats.empty()) {
        // In "all" mode generate the full report immediately.
        if (mode == "all") {
            print_performance_report(stats);
            generate_html_report("report.html", stats, csv_path);
        } else {
            // In separate-run mode remind the user to run the report step.
            std::cout << "\n[Hint] Run '" << argv[0]
                      << " report' to merge all results and generate report.html\n";
        }
    }

    MPI_Finalize();
    return 0;
}
