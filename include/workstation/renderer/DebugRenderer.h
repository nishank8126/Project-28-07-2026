#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/math/Matrix4d.h"
#include "workstation/spatial/BoundingBox.h"
#include "workstation/tools/MeasurementTool.h"
#include "workstation/tools/ClipTool.h"
#include "workstation/tools/SectionTool.h"
#include "workstation/tools/SelectionTool.h"

#include <cstdint>
#include <string>
#include <vector>

namespace workstation {
namespace renderer {

struct DebugVertex {
    math::Point3d position;
    uint32_t color;
};

struct DebugLine {
    math::Point3d start;
    math::Point3d end;
    uint32_t color;
    float width = 1.0f;
};

struct DebugBox {
    spatial::BoundingBox bounds;
    uint32_t color;
    float lineWidth = 1.0f;
};

struct DebugText {
    math::Point3d position;
    std::string text;
    uint32_t color;
    float scale = 1.0f;
};

class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer() = default;

    void Initialize();
    void Shutdown();

    void DrawLine(const math::Point3d& start, const math::Point3d& end, uint32_t color, float width = 1.0f);
    void DrawBox(const spatial::BoundingBox& box, uint32_t color, float lineWidth = 1.0f);
    void DrawPoint(const math::Point3d& position, uint32_t color, float size = 3.0f);
    void DrawText(const math::Point3d& position, const std::string& text, uint32_t color, float scale = 1.0f);
    void DrawPlane(const math::Point3d& center, const math::Point3d& normal, double size, uint32_t color);
    void DrawPolygon(const std::vector<math::Point3d>& points, uint32_t color, float width = 1.0f);
    void DrawCircle(const math::Point3d& center, const math::Point3d& normal, double radius, uint32_t color, float width = 1.0f);

    void DrawSelection(const tools::SelectionResult& selection);
    void DrawMeasurement(const tools::MeasurementResult& measurement);
    void DrawClipPlane(const tools::ClipPlane& plane, double size);
    void DrawClipBox(const tools::ClipBox& box);
    void DrawSectionLine(const tools::SectionLine& line, uint32_t color);

    const std::vector<DebugLine>& GetLines() const { return lines_; }
    const std::vector<DebugBox>& GetBoxes() const { return boxes_; }
    const std::vector<DebugText>& GetTexts() const { return texts_; }

    void Clear();

private:
    std::vector<DebugLine> lines_;
    std::vector<DebugBox> boxes_;
    std::vector<DebugText> texts_;

    math::Point3d TransformPoint(const math::Point3d& p, const math::Matrix4d& mvp) const;
};

} // namespace renderer
} // namespace workstation
