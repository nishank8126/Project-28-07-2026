#pragma once
#include "workstation/core/Element.h"
#include "workstation/display/ViewContext.h"
#include "workstation/display/GraphicsRecorder.h"

namespace workstation {

// Clean equivalent of the graphics producer / stroker role (RE: 4,
// IStrokeForCache). Converts an Element's CAD meaning into display output by
// emitting commands into a GraphicsRecorder. DO NOT add actual Line/Arc/Mesh
// APIs yet (later pieces). Only the role is modeled here.
class IElementGraphicsProvider {
public:
    virtual ~IElementGraphicsProvider() = default;

    virtual void EmitGraphics(const Element& element,
                              ViewContext& view,
                              GraphicsRecorder& recorder) const = 0;
};

} // namespace workstation
