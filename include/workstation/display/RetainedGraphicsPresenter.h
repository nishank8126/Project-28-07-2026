#pragma once
#include "workstation/display/ViewContext.h"
#include "workstation/display/IViewOutput.h"
#include "workstation/display/SubmissionTokens.h"
#include "workstation/display/ElementRenderOverrides.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/ElementGraphicsService.h"
#include "workstation/math/Transform3d.h"
#include <optional>
#include <cstddef>

namespace workstation {

// GraphicsRetention / GraphicsResolveResult are defined in
// ElementGraphicsService.h (the resolve layer owns that vocabulary).

enum class RetainedSubmissionPath {
    PathA,
    PathB
};

struct RetainedSubmissionRequest {
    CachedGraphicsHandle            graphics;
    std::optional<math::Transform3d> transform;
    std::optional<ClipVolumeToken>  clip;
    RetainedSubmissionPath          path = RetainedSubmissionPath::PathA;
    bool                            augmentOverrides = false;
    bool                            enableAuxiliaryOverride = false;
};

struct PresentationStats {
    size_t submissions = 0;
    size_t pathASubmissions = 0;
    size_t pathBSubmissions = 0;
    size_t transientSubmissions = 0;
};

// Equivalent of DrawQvElem / _DrawQvElem submission stage. It does NOT perform
// cache resolution (that stays in ElementGraphicsService). It only manages the
// transform/clip push, temporary overrides, and PathA/PathB submission to the
// renderer-independent IViewOutput.
class RetainedGraphicsPresenter {
    PresentationStats m_stats;

public:
    void Present(ViewContext& view, const RetainedSubmissionRequest& request);
    void MarkTransient() { ++m_stats.transientSubmissions; }
    const PresentationStats& Stats() const { return m_stats; }
};

} // namespace workstation
