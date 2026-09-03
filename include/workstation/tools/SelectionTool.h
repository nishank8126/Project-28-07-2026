#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/spatial/BoundingBox.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace workstation {
namespace tools {

enum class SelectionMode {
    Point,
    Rectangle,
    Polygon,
    Box,
    Radius,
    Fence,   // polyline fence — select all points within the fence corridor
    None
};

struct SelectionResult {
    std::vector<uint64_t> selectedNodeIds;
    std::vector<math::Point3d> selectedPositions;
    uint64_t totalSelectedPoints = 0;
    spatial::BoundingBox selectionBounds;
};

struct SelectionCallback {
    std::function<void(const SelectionResult&)> onSelectionChanged;
    std::function<void()> onSelectionCleared;
};

class SelectionTool {
public:
    SelectionTool() = default;
    ~SelectionTool() = default;

    void SetSelectionMode(SelectionMode mode) { mode_ = mode; }
    SelectionMode GetSelectionMode() const { return mode_; }

    void SetCallback(const SelectionCallback& cb) { callback_ = cb; }

    void BeginSelection(const math::Point3d& origin);
    void UpdateSelection(const math::Point3d& current);
    void EndSelection(const math::Point3d& end);
    void CancelSelection();

    void AddPolygonPoint(const math::Point3d& point);
    void FinishPolygon();

    const SelectionResult& GetResult() const { return result_; }
    bool HasSelection() const { return result_.totalSelectedPoints > 0; }
    void ClearSelection();

    void SetRadius(double radius) { radius_ = radius; }
    double GetRadius() const { return radius_; }

    void SetFenceWidth(double w) { fenceWidth_ = w; }
    double GetFenceWidth() const { return fenceWidth_; }

    void RenderUI();

private:
    SelectionMode mode_ = SelectionMode::None;
    SelectionResult result_;
    SelectionCallback callback_;

    math::Point3d start_;
    math::Point3d current_;
    std::vector<math::Point3d> polygonPoints_;
    bool selecting_ = false;
    double radius_ = 10.0;
    double fenceWidth_ = 5.0;

    spatial::BoundingBox ComputeAABB() const;
    bool IsPointInPolygon(const math::Point3d& p) const;
    bool IsPointInBox(const math::Point3d& p, const spatial::BoundingBox& box) const;
};

} // namespace tools
} // namespace workstation
