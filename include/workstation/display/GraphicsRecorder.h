#pragma once
#include "workstation/core/IdTypes.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/RecordedCommand.h"
#include <vector>
#include <stdexcept>

namespace workstation {

// Receives graphics commands emitted during element graphics generation and
// produces a renderer-independent CachedGraphics at EndElement. It deliberately
// holds NO GPU state (RE rule S / finding 8). The recorded commands are only a
// stand-in to validate the pipeline; the final geometry packet is later.
class GraphicsRecorder {
    bool m_begun = false;
    ElementId m_elementId;
    uint64_t  m_revision = 0;
    std::vector<RecordedGraphicsCommand> m_commands;

public:
    void BeginElement(ElementId id, uint64_t geometryRevision);
    void EmitCommand(uint32_t type, uint64_t payload);
    CachedGraphicsHandle EndElement();

    bool  IsRecording() const { return m_begun; }
    size_t CommandCount() const { return m_commands.size(); }
};

} // namespace workstation
