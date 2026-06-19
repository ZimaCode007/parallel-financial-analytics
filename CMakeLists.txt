cmake_minimum_required(VERSION 3.18)
project(MCP_final CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenMP ─────────────────────────────────────────────────────────────
if(APPLE)
    set(OpenMP_C_FLAGS   "-Xpreprocessor -fopenmp" CACHE STRING "" FORCE)
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp" CACHE STRING "" FORCE)
    set(OpenMP_C_LIB_NAMES   "omp" CACHE STRING "" FORCE)
    set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "" FORCE)
    set(OpenMP_omp_LIBRARY
        "/opt/homebrew/opt/libomp/lib/libomp.dylib" CACHE FILEPATH "" FORCE)
    list(APPEND EXTRA_INCLUDES /opt/homebrew/opt/libomp/include)
endif()
find_package(OpenMP REQUIRED)

# ── MPI ────────────────────────────────────────────────────────────────
find_package(MPI REQUIRED)

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
    ${EXTRA_INCLUDES}
)

target_compile_options(MCP_final PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Release>>:-O3>
    $<$<AND:$<NOT:$<PLATFORM_ID:Windows>>,$<CONFIG:Release>>:-march=native>
)

target_link_libraries(MCP_final PRIVATE
    OpenMP::OpenMP_CXX
    MPI::MPI_CXX
)
