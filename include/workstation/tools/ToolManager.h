#pragma once
#include "workstation/tools/SelectionTool.h"
#include "workstation/tools/MeasurementTool.h"
#include "workstation/tools/ClipTool.h"
#include "workstation/tools/SectionTool.h"
#include "workstation/tools/CrossSectionTool.h"
#include "workstation/tools/ClassificationTool.h"
#include "workstation/tools/CadDrawTools.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
    // CAD Drawing tools
    DrawPoint,
    DrawLine,
    DrawPolyline,
    DrawPolygon,
    DrawRectangle,
    DrawCircle,
    DrawArc,
    DrawText,
    DrawDimension,
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

    CadDrawTool* GetDrawTool();
    const std::vector<CadGeometry>& GetDrawnGeometries() const { return drawnGeometries_; }
    void ClearDrawnGeometries() { drawnGeometries_.clear(); }

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

    // CAD Drawing tools
    std::unique_ptr<PointDrawTool> drawPoint_;
    std::unique_ptr<LineDrawTool> drawLine_;
    std::unique_ptr<PolylineDrawTool> drawPolyline_;
    std::unique_ptr<PolygonDrawTool> drawPolygon_;
    std::unique_ptr<RectangleDrawTool> drawRect_;
    std::unique_ptr<CircleDrawTool> drawCircle_;
    std::unique_ptr<ArcDrawTool> drawArc_;
    std::unique_ptr<TextDrawTool> drawText_;
    std::unique_ptr<DimensionDrawTool> drawDim_;

    std::vector<CadGeometry> drawnGeometries_;
};

} // namespace tools
} // namespace workstation
