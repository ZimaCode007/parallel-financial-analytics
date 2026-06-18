#pragma once

#include <vector>
#include "transaction.h"
#include "analytics.h"

/**
 * MPI Execution Module
 *
 * Implements the distributed-memory parallel analytics pipeline using MPI.
 *
 * Execution model:
 *   - Rank 0 loads the full dataset, partitions it, and scatters chunks to
 *     all ranks using MPI_Scatterv (variable-length chunks).
 *   - Each rank computes local aggregates (SUM, MAX, COUNT, group-by maps)
 *     over its own chunk.
 *   - Rank 0 gathers scalar results with MPI_Reduce and collects group-by
 *     maps via MPI_Gather + serialisation.
 *   - Only rank 0 returns a valid AnalyticsResult; other ranks return a
 *     default-constructed result.
 *
 * Serialisation of group-by maps:
 *   Maps are serialised to a flat byte buffer (key_len|key|value pairs) so
 *   they can be sent with MPI_Send/Recv without a custom MPI datatype.
 *
 * Usage (called on every rank):
 *   MpiEngine engine;
 *   auto result = engine.run(records);  // records only meaningful on rank 0
 *   if (engine.is_root()) { ... }
 */
class MpiEngine {
public:
    /** Initialise and cache MPI rank / world size. */
    MpiEngine();

    /** Run analytics; scatters work across all ranks and reduces on root. */
    Analytics::AnalyticsResult run(const std::vector<Transaction>& records) const;

    bool is_root()   const { return rank_ == 0; }
    int  rank()      const { return rank_; }
    int  world_size() const { return world_size_; }

private:
    int rank_;
    int world_size_;

    /* ── Serialisation helpers ── */

    /** Serialise a group-by map to a byte vector. */
    static std::vector<char> serialise_map(
        const std::unordered_map<std::string, long long>& m);

    /** Deserialise a byte vector back into a group-by map. */
    static std::unordered_map<std::string, long long> deserialise_map(
        const std::vector<char>& buf);
};
