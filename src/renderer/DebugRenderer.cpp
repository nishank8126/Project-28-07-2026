#include "workstation/renderer/DebugRenderer.h"
#include "imgui.h"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace workstation {
namespace renderer {

void DebugRenderer::Initialize() {
    Clear();
}

void DebugRenderer::Shutdown() {
    Clear();
}

void DebugRenderer::DrawLine(const math::Point3d& start, const math::Point3d& end, uint32_t color, float width) {
    lines_.push_back({start, end, color, width});
}

void DebugRenderer::DrawBox(const spatial::BoundingBox& box, uint32_t color, float lineWidth) {
    boxes_.push_back({box, color, lineWidth});
}

void DebugRenderer::DrawPoint(const math::Point3d& position, uint32_t color, float size) {
    DebugLine line;
    line.start = position;
    line.end = position;
    line.color = color;
    line.width = size;
    lines_.push_back(line);
}

void DebugRenderer::DrawText(const math::Point3d& position, const std::string& text, uint32_t color, float scale) {
    texts_.push_back({position, text, color, scale});
}

void DebugRenderer::DrawPlane(const math::Point3d& center, const math::Point3d& normal, double size, uint32_t color) {
    math::Point3d up{0.0, 0.0, 1.0};
    if (std::abs(normal.z) > 0.99) {
        up = math::Point3d{1.0, 0.0, 0.0};
    }

    math::Point3d right{
        up.y * normal.z - up.z * normal.y,
        up.z * normal.x - up.x * normal.z,
        up.x * normal.y - up.y * normal.x
    };
    double len = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (len > 1e-10) {
        right.x /= len;
        right.y /= len;
        right.z /= len;
    }

    math::Point3d fwd{
        normal.y * right.z - normal.z * right.y,
        normal.z * right.x - normal.x * right.z,
        normal.x * right.y - normal.y * right.x
    };

    math::Point3d corners[4] = {
        {center.x + right.x * size + fwd.x * size,
         center.y + right.y * size + fwd.y * size,
         center.z + right.z * size + fwd.z * size},
        {center.x - right.x * size + fwd.x * size,
         center.y - right.y * size + fwd.y * size,
         center.z - right.z * size + fwd.z * size},
        {center.x - right.x * size - fwd.x * size,
         center.y - right.y * size - fwd.y * size,
         center.z - right.z * size - fwd.z * size},
        {center.x + right.x * size - fwd.x * size,
         center.y + right.y * size - fwd.y * size,
         center.z + right.z * size - fwd.z * size}
    };

    DrawLine(corners[0], corners[1], color, 2.0f);
    DrawLine(corners[1], corners[2], color, 2.0f);
    DrawLine(corners[2], corners[3], color, 2.0f);
    DrawLine(corners[3], corners[0], color, 2.0f);
}

void DebugRenderer::DrawPolygon(const std::vector<math::Point3d>& points, uint32_t color, float width) {
    for (size_t i = 0; i < points.size(); ++i) {
        size_t next = (i + 1) % points.size();
        DrawLine(points[i], points[next], color, width);
    }
}

void DebugRenderer::DrawCircle(const math::Point3d& center, const math::Point3d& normal, double radius, uint32_t color, float width) {
    const int segments = 32;
    std::vector<math::Point3d> circlePoints(segments);

    math::Point3d up{0.0, 0.0, 1.0};
    if (std::abs(normal.z) > 0.99) {
        up = math::Point3d{1.0, 0.0, 0.0};
    }

    math::Point3d right{
        up.y * normal.z - up.z * normal.y,
        up.z * normal.x - up.x * normal.z,
        up.x * normal.y - up.y * normal.x
    };
    double len = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (len > 1e-10) {
        right.x /= len;
        right.y /= len;
        right.z /= len;
    }

    math::Point3d fwd{
        normal.y * right.z - normal.z * right.y,
        normal.z * right.x - normal.x * normal.z,
        normal.x * right.y - normal.y * right.x
    };

    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * 3.14159265358979323846 * i / segments;
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);

        circlePoints[i] = math::Point3d{
            center.x + (right.x * cosA + fwd.x * sinA) * radius,
            center.y + (right.y * cosA + fwd.y * sinA) * radius,
            center.z + (right.z * cosA + fwd.z * sinA) * radius
        };
    }

    DrawPolygon(circlePoints, color, width);
}

void DebugRenderer::DrawSelection(const tools::SelectionResult& selection) {
    if (selection.totalSelectedPoints == 0) return;

    DrawBox(selection.selectionBounds, 0xFF00FF00, 2.0f);

    std::ostringstream oss;
    oss << "Selected: " << selection.totalSelectedPoints << " points";
    DrawText(math::Point3d{
        selection.selectionBounds.minX,
        selection.selectionBounds.minY,
        selection.selectionBounds.maxZ + 1.0
    }, oss.str(), 0xFF00FF00);
}

void DebugRenderer::DrawMeasurement(const tools::MeasurementResult& measurement) {
    if (measurement.points.empty()) return;

    if (measurement.mode == tools::MeasurementMode::Distance && measurement.points.size() >= 2) {
        for (size_t i = 0; i + 1 < measurement.points.size(); ++i) {
            DrawLine(measurement.points[i].position, measurement.points[i + 1].position, 0xFFFF0000, 3.0f);
        }

        math::Point3d mid{
            (measurement.points[0].position.x + measurement.points.back().position.x) * 0.5,
            (measurement.points[0].position.y + measurement.points.back().position.y) * 0.5,
            (measurement.points[0].position.z + measurement.points.back().position.z) * 0.5
        };

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << measurement.distance << " m";
        DrawText(mid, oss.str(), 0xFFFF0000, 1.5f);
    } else if (measurement.mode == tools::MeasurementMode::Area && measurement.points.size() >= 3) {
        std::vector<math::Point3d> polyPoints;
        for (const auto& pt : measurement.points) {
            polyPoints.push_back(pt.position);
        }
        DrawPolygon(polyPoints, 0xFF00FFFF, 2.0f);

        math::Point3d center{0.0, 0.0, 0.0};
        for (const auto& pt : measurement.points) {
            center.x += pt.position.x;
            center.y += pt.position.y;
            center.z += pt.position.z;
        }
        double n = static_cast<double>(measurement.points.size());
        center.x /= n;
        center.y /= n;
        center.z /= n;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << measurement.area << " m²";
        DrawText(center, oss.str(), 0xFF00FFFF, 1.5f);
    }
}

void DebugRenderer::DrawClipPlane(const tools::ClipPlane& plane, double size) {
    if (!plane.enabled) return;

    DrawPlane(plane.point, plane.normal, size, 0xFFFF00FF);
}

void DebugRenderer::DrawClipBox(const tools::ClipBox& box) {
    if (!box.enabled) return;

    DrawBox(box.bounds, 0xFFFF00FF, 2.0f);
}

void DebugRenderer::DrawSectionLine(const tools::SectionLine& line, uint32_t color) {
    DrawLine(line.start, line.end, color, 3.0f);

    math::Point3d mid{
        (line.start.x + line.end.x) * 0.5,
        (line.start.y + line.end.y) * 0.5,
        (line.start.z + line.end.z) * 0.5
    };

    std::ostringstream oss;
    oss << "Section (w=" << std::fixed << std::setprecision(1) << line.width << "m)";
    DrawText(mid, oss.str(), color, 1.2f);
}

void DebugRenderer::Clear() {
    lines_.clear();
    boxes_.clear();
    texts_.clear();
}

math::Point3d DebugRenderer::TransformPoint(const math::Point3d& p, const math::Matrix4d& mvp) const {
    return mvp.MultiplyAndRenormalize(p);
}

} // namespace renderer
} // namespace workstation
