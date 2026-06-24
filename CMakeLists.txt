cmake_minimum_required(VERSION 3.18)
project(MCP_final CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── OpenMP ─────────────────────────────────────────────────────────────
# Apple Clang does not bundle an OpenMP runtime; libomp from Homebrew provides it.
if(APPLE)
    set(OpenMP_C_FLAGS   "-Xpreprocessor -fopenmp" CACHE STRING "" FORCE)
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp" CACHE STRING "" FORCE)
    set(OpenMP_C_LIB_NAMES   "omp" CACHE STRING "" FORCE)
    set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "" FORCE)
    set(OpenMP_omp_LIBRARY
        "/opt/homebrew/opt/libomp/lib/libomp.dylib" CACHE FILEPATH "" FORCE)
endif()
find_package(OpenMP REQUIRED)

# ── MPI ────────────────────────────────────────────────────────────────
# When the project is built with mpicc/mpicxx as the compiler, MPI headers
# and libraries are already baked into the wrapper, so we locate them
# directly instead of relying on FindMPI (which re-probes and can fail).
find_program(MPICXX_BIN mpicxx REQUIRED
    HINTS /opt/homebrew/bin /usr/local/bin)

execute_process(
    COMMAND ${MPICXX_BIN} --showme:compile
    OUTPUT_VARIABLE MPI_COMPILE_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${MPICXX_BIN} --showme:link
    OUTPUT_VARIABLE MPI_LINK_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND ${MPICXX_BIN} --showme:incdirs
    OUTPUT_VARIABLE MPI_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)

# ── Source files ───────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    src/data_loader.cpp
    src/data_partitioner.cpp
    src/analytics.cpp
    src/omp_engine.cpp
    src/mpi_engine.cpp
    src/performance.cpp
    src/report_generator.cpp
    src/result_io.cpp
)

# ── Executable ─────────────────────────────────────────────────────────
add_executable(MCP_final ${SOURCES})

target_include_directories(MCP_final PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    /opt/homebrew/opt/libomp/include
    ${MPI_INCLUDE_DIRS}
)

# Pass MPI compile/link flags as raw strings (they come from the wrapper).
separate_arguments(MPI_COMPILE_LIST UNIX_COMMAND "${MPI_COMPILE_FLAGS}")
separate_arguments(MPI_LINK_LIST    UNIX_COMMAND "${MPI_LINK_FLAGS}")

target_compile_options(MCP_final PRIVATE
    ${MPI_COMPILE_LIST}
    $<$<CONFIG:Release>:-O3 -march=native>
)

target_link_options(MCP_final PRIVATE ${MPI_LINK_LIST})

target_link_libraries(MCP_final PRIVATE
    OpenMP::OpenMP_CXX
)
