#include "workstation/renderer/SelectionRenderer.h"

namespace workstation {
namespace renderer {

void SelectionRenderer::UpdateSelection(const std::vector<scene::SelectionResult>& selections) {
    selections_ = selections;
    needsUpdate_ = true;
}

void SelectionRenderer::RenderOverlay(OverlayRenderer& overlay, const Camera& camera) {
    (void)camera;

    for (const auto& sel : selections_) {
        if (!sel.valid) continue;

        if (sel.objectType == scene::ObjectType::CadAttachment) {
            overlay.DrawHighlightLine(
                sel.worldPosition.x - 0.5f, sel.worldPosition.y, sel.worldPosition.z,
                sel.worldPosition.x + 0.5f, sel.worldPosition.y, sel.worldPosition.z,
                highlightWidth_);
            overlay.DrawHighlightLine(
                sel.worldPosition.x, sel.worldPosition.y - 0.5f, sel.worldPosition.z,
                sel.worldPosition.x, sel.worldPosition.y + 0.5f, sel.worldPosition.z,
                highlightWidth_);
        } else {
            overlay.DrawHighlightPoint(sel.worldPosition.x, sel.worldPosition.y, sel.worldPosition.z,
                                        pointHighlightSize_);
        }
    }

    needsUpdate_ = false;
}

} // namespace renderer
} // namespace workstation
