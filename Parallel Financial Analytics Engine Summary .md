# Parallel Financial Analytics Engine Summary

**Nguyen Phu Pham**
**Roshika Pant**
**Bingqian Yang**

## Project Overview

The objective of this project is to design and implement a **Parallel Financial Analytics Engine** capable of processing large-scale financial transaction datasets efficiently using parallel computing techniques. The project will utilize the **IBM Credit Card Transactions Dataset**, which contains millions of transaction records, to simulate real-world financial data processing scenarios.

As modern financial institutions generate massive volumes of transaction data every day, traditional sequential processing methods often become a performance bottleneck. To address this challenge, this project will explore both **shared-memory parallelism** and **distributed-memory parallelism** through the use of **OpenMP** and **MPI**, respectively.

## Project Objectives

The system will be developed in C++ and will provide the following core functionalities:

- Loading and parsing large CSV-based transaction datasets
- Partitioning datasets for parallel processing
- Performing financial analytics operations, including:
  - Total transaction amount (SUM)
  - Average transaction amount (AVG)
  - Maximum transaction value (MAX)
  - Transaction count (COUNT)
  - Group-by analysis based on merchant category and state
- Supporting both OpenMP-based and MPI-based implementations
- Measuring and comparing performance across different execution models

## System Architecture

The system will consist of several modules:

1. **Data Loading Module** – Reads and preprocesses transaction records from CSV files.
2. **Data Partitioning Module** – Divides data into chunks for parallel execution.
3. **OpenMP Execution Module** – Executes analytics tasks using multi-threading on shared-memory systems.
4. **MPI Execution Module** – Executes analytics tasks using multiple processes in a distributed environment.
5. **Financial Analytics Module** – Performs aggregation and grouping operations on transaction data.
6. **Performance Evaluation Module** – Measures execution time, speedup, scalability, and efficiency.

## Experimental Evaluation

To evaluate the effectiveness of parallelization, the project will compare:

- Sequential execution
- OpenMP implementation
- MPI implementation

Experiments will be conducted using datasets of different sizes and varying numbers of threads/processes. Performance metrics such as execution time, speedup, and scalability will be analyzed to identify the advantages and limitations of each parallel programming model.

## Expected Outcomes

By the end of the project, the team will have developed a functional financial analytics engine that demonstrates practical applications of parallel computing techniques in large-scale data processing. The project will provide hands-on experience with OpenMP, MPI, performance optimization, and scalability analysis while reflecting real-world challenges commonly found in financial data analytics systems.