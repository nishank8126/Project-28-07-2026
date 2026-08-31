#include "workstation/display/GraphicsRecorder.h"

namespace workstation {

void GraphicsRecorder::BeginElement(ElementId id, uint64_t geometryRevision) {
    if (m_begun)
        throw std::logic_error("GraphicsRecorder::BeginElement: already recording");
    m_begun = true;
    m_elementId = id;
    m_revision = geometryRevision;
    m_commands.clear();
}

void GraphicsRecorder::EmitCommand(uint32_t type, uint64_t payload) {
    if (!m_begun)
        throw std::logic_error("GraphicsRecorder::EmitCommand: not recording");
    m_commands.push_back(RecordedGraphicsCommand{type, payload});
}

CachedGraphicsHandle GraphicsRecorder::EndElement() {
    if (!m_begun)
        throw std::logic_error("GraphicsRecorder::EndElement: no active recording");

    CachedGraphicsHandle result;
    if (!m_commands.empty()) {
        // RE UNKNOWN: exact proprietary empty-element behavior not proven.
        // OUR Piece 2 policy: an element that emits no commands yields no
        // usable cache entry (nullptr), so it will be regenerated next time.
        result = CreateCachedGraphics(m_elementId, m_revision, m_commands.size());
    }
    m_begun = false;
    m_commands.clear();
    return result;
}

} // namespace workstation
