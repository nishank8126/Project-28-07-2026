#pragma once
#include "workstation/display/ElementGraphicsService.h"
#include "workstation/display/RetainedGraphicsPresenter.h"
#include "workstation/display/SubmissionTokens.h"
#include <optional>

namespace workstation {

// High-level equivalent of _DrawCached: resolve retained graphics, then
// present them. Cache resolution stays in ElementGraphicsService; this class
// only wires resolve -> present and records transient submissions. This keeps
// the brief's required separation (RE S) between cache policy and submission.
struct PresentationDrawRequest {
    GraphicsResolveRequest        resolve;
    std::optional<math::Transform3d> transform;
    std::optional<ClipVolumeToken> clip;
    RetainedSubmissionPath         path = RetainedSubmissionPath::PathA;
    bool                           augmentOverrides = false;
    bool                           enableAuxiliaryOverride = false;
};

class ElementPresentationService {
    ElementGraphicsService&  m_gfx;
    RetainedGraphicsPresenter& m_presenter;

public:
    ElementPresentationService(ElementGraphicsService& g,
                               RetainedGraphicsPresenter& p)
        : m_gfx(g), m_presenter(p) {}

    void Draw(ViewContext& view, const PresentationDrawRequest& req) {
        GraphicsResolveResult res = m_gfx.ResolveForPresentation(view, req.resolve);
        if (!res.graphics) return; // nothing to draw

        RetainedSubmissionRequest sub;
        sub.graphics = res.graphics;
        sub.transform = req.transform;
        sub.clip = req.clip;
        sub.path = req.path;
        sub.augmentOverrides = req.augmentOverrides;
        sub.enableAuxiliaryOverride = req.enableAuxiliaryOverride;
        m_presenter.Present(view, sub);

        if (res.retention == GraphicsRetention::Transient)
            m_presenter.MarkTransient();
    }
};

} // namespace workstation
