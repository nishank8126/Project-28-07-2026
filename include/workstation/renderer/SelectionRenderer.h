#pragma once
#include "workstation/scene/SelectionResult.h"
#include "workstation/renderer/OverlayRenderer.h"
#include "workstation/renderer/Camera.h"

#include <vector>
#include <cstdint>

namespace workstation {
namespace renderer {

class SelectionRenderer {
public:
    SelectionRenderer() = default;
    ~SelectionRenderer() = default;

    void Initialize() { initialized_ = true; }
    void Shutdown() { Clear(); initialized_ = false; }
    bool IsInitialized() const { return initialized_; }

    void SetHighlightColor(float r, float g, float b) { highlightR_ = r; highlightG_ = g; highlightB_ = b; }
    void SetHighlightWidth(float w) { highlightWidth_ = w; }
    void SetPointHighlightSize(float s) { pointHighlightSize_ = s; }

    void UpdateSelection(const std::vector<scene::SelectionResult>& selections);

    void RenderOverlay(OverlayRenderer& overlay, const Camera& camera);

    bool HasSelection() const { return !selections_.empty(); }
    uint32_t GetSelectionCount() const { return static_cast<uint32_t>(selections_.size()); }

    void Clear() { selections_.clear(); needsUpdate_ = true; }

private:
    std::vector<scene::SelectionResult> selections_;
    float highlightR_ = 1.0f, highlightG_ = 1.0f, highlightB_ = 0.0f;
    float highlightWidth_ = 3.0f;
    float pointHighlightSize_ = 5.0f;
    bool initialized_ = false;
    bool needsUpdate_ = false;
};

} // namespace renderer
} // namespace workstation
