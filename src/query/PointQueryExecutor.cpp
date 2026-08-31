#include "workstation/query/PointQueryExecutor.h"

namespace workstation { namespace query {

// Exact masking behaviour recovered from FUN_180045040:
//   state = internal state pointer
//   if state exists AND comparison != state  -> return masked state value
//   otherwise                                -> return masked value with valid flag (bit 0) set
uint64_t PointQueryExecutor::GetState(void* comparison) const {
    uintptr_t value = reinterpret_cast<uintptr_t>(internalState_);
    if (value != 0 && comparison != internalState_)
        return (value >> 8) << 8;
    return ((value >> 8) << 8) | 1u;
}

// Visibility test (FUN_180058e40). Placeholder: the exact Bentley frustum plane
// mathematics are NOT yet reversed. Returns true (accept) so the pipeline runs;
// replace this without redesigning the engine once the maths are recovered.
bool PointQueryExecutor::TestVisibility(spatial::SpatialNode& node) {
    (void)node;
    return true;
}

} // namespace query
} // namespace workstation
