#pragma once
#include <cstdint>

namespace workstation {

// Minimal renderer-independent graphics command record.
// It exists ONLY to verify the recording pipeline in Piece 2. The final
// geometry packet format (DrawPacket) is deferred to a later piece. (RE UNKNOWN:
// exact proprietary recorded-command layout not yet reversed.)
struct RecordedGraphicsCommand {
    uint32_t type = 0;
    uint64_t payload = 0;
};

} // namespace workstation
