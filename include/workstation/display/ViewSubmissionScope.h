#pragma once
#include "workstation/display/ViewContext.h"
#include "workstation/display/IViewOutput.h"
#include "workstation/display/SubmissionTokens.h"
#include "workstation/math/Transform3d.h"

namespace workstation {

// RAII wrapper around IViewOutput::PushTransformClip / PopTransformClip.
// Mirrors the manual push/pop sequence of DrawQvElem (RE) but is exception
// safe: PopTransformClip is always called on destruction. Copy/move disabled.
class ViewSubmissionScope {
    ViewContext& m_view;
    bool         m_active = false;

public:
    ViewSubmissionScope(ViewContext& view,
                        const math::Transform3d* transform,
                        const ClipVolumeToken* clip)
        : m_view(view) {
        m_view.output->PushTransformClip(transform, clip);
        m_active = true;
    }

    ~ViewSubmissionScope() {
        if (!m_active) return;
        m_view.output->PopTransformClip();
        m_active = false;
    }

    ViewSubmissionScope(const ViewSubmissionScope&) = delete;
    ViewSubmissionScope& operator=(const ViewSubmissionScope&) = delete;
    ViewSubmissionScope(ViewSubmissionScope&&) = delete;
    ViewSubmissionScope& operator=(ViewSubmissionScope&&) = delete;
};

} // namespace workstation
