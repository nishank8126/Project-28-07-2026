#pragma once
#include "workstation/display/ViewContext.h"
#include "workstation/display/GraphicsRecorder.h"
#include "workstation/display/ElemDisplayParams.h"
#include <utility>

namespace workstation {

// RAII scope that puts a ViewContext into "recording cached graphics" mode for
// the duration of one cache-miss graphics creation (RE: 2/3/K). It saves the
// previous recording flag, active recorder, and display parameters, installs
// the supplied recorder, and restores everything on destruction — even if the
// provider throws. Copy/move are deleted: a scope is bound to exactly one
// stack frame (RE: 11 — recursive cache-recording sessions are prevented, not
// nested).
class GraphicsRecordingScope {
    ViewContext&     m_view;
    bool            m_prevRecording;
    GraphicsRecorder* m_prevRecorder;
    ElemDisplayParams m_prevParams;
    bool            m_active = false;

public:
    GraphicsRecordingScope(ViewContext& view, GraphicsRecorder& recorder)
        : m_view(view),
          m_prevRecording(view.isRecordingCachedGraphics),
          m_prevRecorder(view.activeRecorder),
          m_prevParams(view.currentDisplayParams) {
        m_view.isRecordingCachedGraphics = true;
        m_view.activeRecorder = &recorder;
        m_active = true;
    }

    ~GraphicsRecordingScope() {
        if (!m_active) return;
        m_view.isRecordingCachedGraphics = m_prevRecording;
        m_view.activeRecorder = m_prevRecorder;
        m_view.currentDisplayParams = m_prevParams;
        m_active = false;
    }

    GraphicsRecordingScope(const GraphicsRecordingScope&) = delete;
    GraphicsRecordingScope& operator=(const GraphicsRecordingScope&) = delete;
    GraphicsRecordingScope(GraphicsRecordingScope&&) = delete;
    GraphicsRecordingScope& operator=(GraphicsRecordingScope&&) = delete;
};

} // namespace workstation
