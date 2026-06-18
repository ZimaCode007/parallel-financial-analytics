cmake_minimum_required(VERSION 3.18)
project(MCP_final CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find dependencies ──────────────────────────────────────────────────
find_package(OpenMP REQUIRED)
find_package(MPI    REQUIRED)

# ── Source files ───────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    src/data_loader.cpp
    src/data_partitioner.cpp
    src/analytics.cpp
    src/omp_engine.cpp
    src/mpi_engine.cpp
    src/performance.cpp
)

# ── Executable ─────────────────────────────────────────────────────────
add_executable(MCP_final ${SOURCES})

target_include_directories(MCP_final PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${MPI_CXX_INCLUDE_DIRS}
)

target_link_libraries(MCP_final PRIVATE
    OpenMP::OpenMP_CXX
    MPI::MPI_CXX
)

# Enable aggressive optimisation in Release builds for meaningful benchmarks.
target_compile_options(MCP_final PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
)
