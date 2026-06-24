#include <iostream>
#include <string>
#include <filesystem>
#include <mpi.h>

#include "data_loader.h"
#include "analytics.h"
#include "omp_engine.h"
#include "mpi_engine.h"
#include "performance.h"
#include "report_generator.h"
#include "result_io.h"

/**
 * 程序入口：Parallel Financial Analytics Engine
 *
 * 支持两种运行方式：
 *
 *   方式一（分步运行，互不干扰）：
 *     ./MCP_final <csv_path> seq   [max_rows]                 # 仅跑 Sequential
 *     ./MCP_final <csv_path> omp   [max_rows] [omp_threads]   # 仅跑 OpenMP
 *     mpirun -np 4 ./MCP_final <csv_path> mpi [max_rows]      # 仅跑 MPI
 *     ./MCP_final report                                       # 合并结果生成报告
 *
 *   方式二（一次性全部运行，向后兼容）：
 *     ./MCP_final <csv_path> all   [max_rows] [omp_threads]
 *     mpirun -np 4 ./MCP_final <csv_path> all [max_rows] [omp_threads]
 *
 *   每个分步模式会将结果保存到 results/ 目录下的 .result 文件中。
 *   report 模式读取这些文件并生成合并的性能对比报告和 report.html。
 */

static void print_usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <csv_path> seq   [max_rows] [threads] [sample_ratio]\n"
              << "  " << prog << " <csv_path> omp   [max_rows] [threads] [sample_ratio]\n"
              << "  mpirun -np N " << prog << " <csv_path> mpi [max_rows] [threads] [sample_ratio]\n"
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

    /* ── report 模式：合并已有结果文件并生成报告 ──────────────────── */
    if (arg1 == "report") {
        if (mpi_rank == 0) {
            std::vector<RunStats> stats;
            std::string results_dir = "results";
            // 按固定顺序读取：seq → omp → mpi
            std::string files[] = {
                results_dir + "/seq.result",
                results_dir + "/omp.result",
                results_dir + "/mpi.result"
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
            // 从 seq.result 中无法得知原始 csv_path，用占位符
            generate_html_report("report.html", stats, "(merged from separate runs)");
        }
        MPI_Finalize();
        return 0;
    }

    /* ── 其余模式需要 csv_path 和 mode ───────────────────────────── */
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

    // 创建 results 目录
    if (mpi_rank == 0) {
        std::filesystem::create_directories("results");
    }

    bool run_seq = (mode == "seq" || mode == "all");
    bool run_omp = (mode == "omp" || mode == "all");
    bool run_mpi = (mode == "mpi" || mode == "all");

    /* ── 加载数据（rank 0 加载，可选随机采样） ───────────────────── */
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

    /* ── 输出报告 ─────────────────────────────────────────────────── */
    if (mpi_rank == 0 && !stats.empty()) {
        // all 模式直接生成完整报告
        if (mode == "all") {
            print_performance_report(stats);
            generate_html_report("report.html", stats, csv_path);
        } else {
            // 分步模式提示用户最后执行 report
            std::cout << "\n[Hint] Run '" << argv[0]
                      << " report' to merge all results and generate report.html\n";
        }
    }

    MPI_Finalize();
    return 0;
}
