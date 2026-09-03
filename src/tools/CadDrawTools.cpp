#include "workstation/tools/CadDrawTools.h"
#include <cmath>

namespace workstation { namespace tools {

// ---- PointDrawTool ----
void PointDrawTool::Begin(const math::Point3d& pt) { point_ = pt; active_ = true; complete_ = false; }
void PointDrawTool::Update(const math::Point3d& pt) { if (active_) point_ = pt; }
void PointDrawTool::End(const math::Point3d& pt) { point_ = pt; complete_ = true; }
void PointDrawTool::Cancel() { active_ = false; complete_ = false; }
void PointDrawTool::AddVertex(const math::Point3d& pt) { Begin(pt); End(pt); }
CadGeometry PointDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Point;
    g.vertices.push_back(point_); g.color = color_; return g;
}
CadGeometry PointDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- LineDrawTool ----
void LineDrawTool::Begin(const math::Point3d& pt) { points_.clear(); points_.push_back(pt); active_ = true; complete_ = false; }
void LineDrawTool::Update(const math::Point3d& pt) { /* live preview handled externally */ }
void LineDrawTool::End(const math::Point3d& pt) { points_.push_back(pt); complete_ = true; }
void LineDrawTool::Cancel() { points_.clear(); active_ = false; complete_ = false; }
void LineDrawTool::AddVertex(const math::Point3d& pt) {
    if (points_.empty()) { Begin(pt); } else { points_.push_back(pt); complete_ = true; }
}
CadGeometry LineDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Line; g.vertices = points_; g.color = color_; return g;
}
CadGeometry LineDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- PolylineDrawTool ----
void PolylineDrawTool::Begin(const math::Point3d& pt) { points_.clear(); points_.push_back(pt); active_ = true; complete_ = false; }
void PolylineDrawTool::Update(const math::Point3d& pt) { /* live preview */ }
void PolylineDrawTool::End(const math::Point3d& pt) { points_.push_back(pt); complete_ = true; }
void PolylineDrawTool::Cancel() { points_.clear(); active_ = false; complete_ = false; }
void PolylineDrawTool::AddVertex(const math::Point3d& pt) {
    if (!active_) Begin(pt); else points_.push_back(pt);
}
CadGeometry PolylineDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Polyline; g.vertices = points_; g.color = color_; return g;
}
CadGeometry PolylineDrawTool::Finish() { active_ = false; complete_ = true; return GetPreview(); }

// ---- PolygonDrawTool ----
void PolygonDrawTool::Begin(const math::Point3d& pt) { points_.clear(); points_.push_back(pt); active_ = true; complete_ = false; }
void PolygonDrawTool::Update(const math::Point3d& pt) { /* live preview */ }
void PolygonDrawTool::End(const math::Point3d& pt) { points_.push_back(pt); complete_ = true; }
void PolygonDrawTool::Cancel() { points_.clear(); active_ = false; complete_ = false; }
void PolygonDrawTool::AddVertex(const math::Point3d& pt) {
    if (!active_) Begin(pt); else points_.push_back(pt);
}
CadGeometry PolygonDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Polygon; g.vertices = points_; g.closed = true; g.color = color_; return g;
}
CadGeometry PolygonDrawTool::Finish() { active_ = false; complete_ = true; return GetPreview(); }

// ---- RectangleDrawTool ----
void RectangleDrawTool::Begin(const math::Point3d& pt) { start_ = pt; active_ = true; complete_ = false; }
void RectangleDrawTool::Update(const math::Point3d& pt) { if (active_) end_ = pt; }
void RectangleDrawTool::End(const math::Point3d& pt) { end_ = pt; complete_ = true; }
void RectangleDrawTool::Cancel() { active_ = false; complete_ = false; }
void RectangleDrawTool::AddVertex(const math::Point3d& pt) { Begin(pt); }
CadGeometry RectangleDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Rectangle; g.color = color_;
    g.vertices.resize(5);
    g.vertices[0] = {start_.x, start_.y, start_.z};
    g.vertices[1] = {end_.x, start_.y, start_.z};
    g.vertices[2] = {end_.x, end_.y, end_.z};
    g.vertices[3] = {start_.x, end_.y, end_.z};
    g.vertices[4] = g.vertices[0];
    return g;
}
CadGeometry RectangleDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- CircleDrawTool ----
void CircleDrawTool::Begin(const math::Point3d& pt) { center_ = pt; active_ = true; complete_ = false; }
void CircleDrawTool::Update(const math::Point3d& pt) { if (active_) edge_ = pt; }
void CircleDrawTool::End(const math::Point3d& pt) { edge_ = pt; complete_ = true; }
void CircleDrawTool::Cancel() { active_ = false; complete_ = false; }
void CircleDrawTool::AddVertex(const math::Point3d& pt) { Begin(pt); }
CadGeometry CircleDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Circle; g.color = color_;
    g.vertices.push_back(center_);
    g.radius = std::sqrt((edge_.x-center_.x)*(edge_.x-center_.x) +
                          (edge_.y-center_.y)*(edge_.y-center_.y) +
                          (edge_.z-center_.z)*(edge_.z-center_.z));
    return g;
}
CadGeometry CircleDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- ArcDrawTool (3-point arc) ----
void ArcDrawTool::Begin(const math::Point3d& pt) { points_.clear(); points_.push_back(pt); active_ = true; complete_ = false; step_ = 0; }
void ArcDrawTool::Update(const math::Point3d& pt) { /* live preview */ }
void ArcDrawTool::End(const math::Point3d& pt) { points_.push_back(pt); step_++; if (step_ >= 2) complete_ = true; }
void ArcDrawTool::Cancel() { points_.clear(); active_ = false; complete_ = false; step_ = 0; }
void ArcDrawTool::AddVertex(const math::Point3d& pt) { End(pt); }
CadGeometry ArcDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Arc; g.vertices = points_; g.color = color_; return g;
}
CadGeometry ArcDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- TextDrawTool ----
void TextDrawTool::Begin(const math::Point3d& pt) { position_ = pt; active_ = true; complete_ = false; }
void TextDrawTool::Update(const math::Point3d& pt) { if (active_) position_ = pt; }
void TextDrawTool::End(const math::Point3d& pt) { position_ = pt; complete_ = true; }
void TextDrawTool::Cancel() { active_ = false; complete_ = false; }
void TextDrawTool::AddVertex(const math::Point3d& pt) { Begin(pt); End(pt); }
CadGeometry TextDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Text; g.vertices.push_back(position_);
    g.text = text_; g.textHeight = textHeight_; g.color = color_; return g;
}
CadGeometry TextDrawTool::Finish() { active_ = false; return GetPreview(); }

// ---- DimensionDrawTool ----
void DimensionDrawTool::Begin(const math::Point3d& pt) { start_ = pt; active_ = true; complete_ = false; }
void DimensionDrawTool::Update(const math::Point3d& pt) { if (active_) end_ = pt; }
void DimensionDrawTool::End(const math::Point3d& pt) { end_ = pt; complete_ = true; }
void DimensionDrawTool::Cancel() { active_ = false; complete_ = false; }
void DimensionDrawTool::AddVertex(const math::Point3d& pt) { Begin(pt); }
CadGeometry DimensionDrawTool::GetPreview() const {
    CadGeometry g; g.type = CadGeometryType::Dimension; g.color = color_;
    g.vertices.push_back(start_); g.vertices.push_back(end_);
    double dx = end_.x - start_.x, dy = end_.y - start_.y, dz = end_.z - start_.z;
    g.dimensionValue = std::sqrt(dx*dx + dy*dy + dz*dz);
    char buf[64]; snprintf(buf, sizeof(buf), "%.3f", g.dimensionValue);
    g.dimensionLabel = buf; return g;
}
CadGeometry DimensionDrawTool::Finish() { active_ = false; return GetPreview(); }

} // namespace tools
} // namespace workstation
