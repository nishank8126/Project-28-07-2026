#pragma once
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/ElementRenderOverrides.h"
#include "workstation/display/SubmissionTokens.h"
#include "workstation/math/Transform3d.h"

namespace workstation {

// Renderer-independent abstraction for the output sink that retained graphics
// are submitted to. There is NO D3D11 implementation in Piece 3/4 (RE rule S).
// The name IViewOutput mirrors the recovered export-surface concept.
//
// PathA / PathB are deliberately NEUTRAL names: the exact proprietary
// semantic meaning of the two retained submission paths and the extra
// ViewContext-derived scalar are UNKNOWN (RE). They must not be called
// 2D/3D, annotation/world, screen/model, etc. until proven.
class IViewOutput {
public:
    virtual ~IViewOutput() = default;

    virtual void PushTransformClip(const math::Transform3d* transform,
                                   const ClipVolumeToken* clip) = 0;

    virtual void PopTransformClip() = 0;

    virtual void ApplyElementOverrides(const ElementRenderOverrides& overrides) = 0;

    virtual void SubmitRetainedPathA(const CachedGraphicsHandle& graphics) = 0;

    virtual void SubmitRetainedPathB(const CachedGraphicsHandle& graphics,
                                     double viewParameter) = 0;
};

} // namespace workstation
