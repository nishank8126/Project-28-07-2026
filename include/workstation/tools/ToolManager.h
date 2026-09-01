#pragma once
#include "workstation/tools/SelectionTool.h"
#include "workstation/tools/MeasurementTool.h"
#include "workstation/tools/ClipTool.h"
#include "workstation/tools/SectionTool.h"
#include "workstation/tools/CrossSectionTool.h"
#include "workstation/tools/ClassificationTool.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace workstation {

namespace renderer {
class RenderContext;
}
class ImGuiOverlay;

namespace tools {

enum class ToolType {
    Selection,
    Measurement,
    Clip,
    Section,
    CrossSection,
    Classification,
    None
};

class ToolManager {
public:
    ToolManager() = default;
    ~ToolManager() = default;

    void Initialize();
    void Shutdown();

    void SetActiveTool(ToolType type);
    ToolType GetActiveTool() const { return activeTool_; }

    SelectionTool* GetSelectionTool() { return selectionTool_.get(); }
    MeasurementTool* GetMeasurementTool() { return measurementTool_.get(); }
    ClipTool* GetClipTool() { return clipTool_.get(); }
    SectionTool* GetSectionTool() { return sectionTool_.get(); }
    CrossSectionTool* GetCrossSectionTool() { return crossSectionTool_.get(); }
    ClassificationTool* GetClassificationTool() { return classificationTool_.get(); }

    void RenderUI(renderer::RenderContext& ctx);

    const char* GetToolName(ToolType type) const;

private:
    ToolType activeTool_ = ToolType::None;

    std::unique_ptr<SelectionTool> selectionTool_;
    std::unique_ptr<MeasurementTool> measurementTool_;
    std::unique_ptr<ClipTool> clipTool_;
    std::unique_ptr<SectionTool> sectionTool_;
    std::unique_ptr<CrossSectionTool> crossSectionTool_;
    std::unique_ptr<ClassificationTool> classificationTool_;
};

} // namespace tools
} // namespace workstation
