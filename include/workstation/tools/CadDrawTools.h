#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/spatial/BoundingBox.h"
#include <cstdint>
#include <string>
#include <vector>

namespace workstation { namespace tools {

// ---- Geometry types produced by CAD drawing tools ----

enum class CadGeometryType {
    Point,
    Line,
    Polyline,
    Polygon,
    Rectangle,
    Circle,
    Arc,
    Text,
    Dimension,
    None
};

// A single CAD entity (drawn geometry).
struct CadGeometry {
    CadGeometryType type = CadGeometryType::None;
    std::vector<math::Point3d> vertices;
    double radius = 0.0;          // Circle/Arc radius
    double startAngle = 0.0;      // Arc start angle (radians)
    double endAngle = 0.0;        // Arc end angle (radians)
    std::string text;             // Text content
    double textHeight = 2.0;      // Text height
    double dimensionValue = 0.0;  // Dimension measurement
    std::string dimensionLabel;   // Dimension annotation text
    uint32_t color = 0xFFFFFFFF;  // ARGB colour
    bool closed = false;          // Polygon closed flag
    spatial::BoundingBox bounds;
};

// ---- Base drawing tool ----

class CadDrawTool {
public:
    virtual ~CadDrawTool() = default;

    virtual CadGeometryType GetType() const = 0;
    virtual const char* GetName() const = 0;

    // Interaction lifecycle
    virtual void Begin(const math::Point3d& startPt) = 0;
    virtual void Update(const math::Point3d& currentPt) = 0;
    virtual void End(const math::Point3d& endPt) = 0;
    virtual void Cancel() = 0;
    virtual void AddVertex(const math::Point3d& pt) = 0;

    virtual bool IsActive() const = 0;
    virtual bool IsComplete() const = 0;

    // Get the geometry being drawn (live preview).
    virtual CadGeometry GetPreview() const = 0;

    // Finalize and return the completed geometry.
    virtual CadGeometry Finish() = 0;

    // Set the current snapping result from the snapping system.
    virtual void SetSnapPoint(const math::Point3d& pt, bool snapped) {}

    // Rendering helpers (called from the viewport).
    virtual void RenderOverlay() {}

    void SetColor(uint32_t c) { color_ = c; }
    uint32_t GetColor() const { return color_; }
    void SetLineWidth(float w) { lineWidth_ = w; }
    float GetLineWidth() const { return lineWidth_; }

protected:
    uint32_t color_ = 0xFFFFFFFF;
    float lineWidth_ = 1.0f;
};

// ---- Concrete tools ----

class PointDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Point; }
    const char* GetName() const override { return "Point"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    math::Point3d point_;
    bool active_ = false;
    bool complete_ = false;
};

class LineDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Line; }
    const char* GetName() const override { return "Line"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    std::vector<math::Point3d> points_;
    bool active_ = false;
    bool complete_ = false;
};

class PolylineDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Polyline; }
    const char* GetName() const override { return "Polyline"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    std::vector<math::Point3d> points_;
    bool active_ = false;
    bool complete_ = false;
};

class PolygonDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Polygon; }
    const char* GetName() const override { return "Polygon"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    std::vector<math::Point3d> points_;
    bool active_ = false;
    bool complete_ = false;
};

class RectangleDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Rectangle; }
    const char* GetName() const override { return "Rectangle"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    math::Point3d start_, end_;
    bool active_ = false;
    bool complete_ = false;
};

class CircleDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Circle; }
    const char* GetName() const override { return "Circle"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    math::Point3d center_, edge_;
    bool active_ = false;
    bool complete_ = false;
};

class ArcDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Arc; }
    const char* GetName() const override { return "Arc"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    std::vector<math::Point3d> points_; // center, start, end
    int step_ = 0;
    bool active_ = false;
    bool complete_ = false;
};

class TextDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Text; }
    const char* GetName() const override { return "Text"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
    void SetText(const std::string& t) { text_ = t; }
    void SetTextHeight(double h) { textHeight_ = h; }
private:
    math::Point3d position_;
    std::string text_;
    double textHeight_ = 2.0;
    bool active_ = false;
    bool complete_ = false;
};

class DimensionDrawTool : public CadDrawTool {
public:
    CadGeometryType GetType() const override { return CadGeometryType::Dimension; }
    const char* GetName() const override { return "Dimension"; }
    void Begin(const math::Point3d& startPt) override;
    void Update(const math::Point3d& currentPt) override;
    void End(const math::Point3d& endPt) override;
    void Cancel() override;
    void AddVertex(const math::Point3d& pt) override;
    bool IsActive() const override { return active_; }
    bool IsComplete() const override { return complete_; }
    CadGeometry GetPreview() const override;
    CadGeometry Finish() override;
private:
    math::Point3d start_, end_;
    bool active_ = false;
    bool complete_ = false;
};

} // namespace tools
} // namespace workstation
