#include "workstation/display/RetainedGraphicsPresenter.h"
#include "workstation/display/ViewSubmissionScope.h"
#include "workstation/display/ElementOverrideScope.h"
#include <stdexcept>

namespace workstation {

void RetainedGraphicsPresenter::Present(ViewContext& view,
                                        const RetainedSubmissionRequest& req) {
    if (!view.output)
        throw std::invalid_argument("RetainedGraphicsPresenter::Present: view.output is null");
    if (!req.graphics)
        return; // RE: no retained graphics -> no submission (TEST 12)

    const math::Transform3d* t = req.transform ? &(*req.transform) : nullptr;
    const ClipVolumeToken* c = req.clip     ? &(*req.clip)     : nullptr;

    ViewSubmissionScope scope(view, t, c);

    // Derive temporary overrides for this submission (OUR simple model).
    ElementRenderOverrides temp = view.currentRenderOverrides;
    if (req.augmentOverrides) {
        temp.lineColor = 0xAA;   // OUR placeholder temporary override
    }
    if (req.enableAuxiliaryOverride) {
        temp.auxiliaryValue = 0xBB; // OUR placeholder auxiliary override
    }

    {
        ElementOverrideScope overrideScope(view, temp);

        switch (req.path) {
        case RetainedSubmissionPath::PathA:
            view.output->SubmitRetainedPathA(req.graphics);
            break;
        case RetainedSubmissionPath::PathB:
            view.output->SubmitRetainedPathB(req.graphics,
                                             view.retainedSubmissionParameter);
            break;
        }
    }

    ++m_stats.submissions;
    if (req.path == RetainedSubmissionPath::PathA) ++m_stats.pathASubmissions;
    else                                          ++m_stats.pathBSubmissions;
}

} // namespace workstation
