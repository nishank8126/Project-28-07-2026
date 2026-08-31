#pragma once
#include <cstdint>

namespace workstation { namespace query {

// Query execution context (Prepare query execution context / ReadPoints query
// context). Holds timing, counters and an optional progress callback. No
// algorithm is invented here; this mirrors the confirmed context concept.
struct QueryContext {
    uint64_t timestampMs = 0;
    uint64_t processedPoints = 0;
    uint64_t lastRequestedPoints = 0;
    uint64_t lastLoadedPoints = 0;
    void (*progress)(uint64_t processed, void* user) = nullptr;
    void* progressUser = nullptr;
};

} // namespace query
} // namespace workstation
