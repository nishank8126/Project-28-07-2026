#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/math/Matrix4d.h"
#include "workstation/spatial/BoundingBox.h"

#include <array>
#include <cstdint>
#include <vector>

namespace workstation {
namespace tools {

enum class ClipPlaneAxis {
    X,
    Y,
    Z,
    Custom
};

struct ClipPlane {
    math::Point3d normal;
    math::Point3d point;
    ClipPlaneAxis axis = ClipPlaneAxis::Z;
    bool enabled = false;
};

struct ClipBox {
    spatial::BoundingBox bounds;
    bool enabled = false;
};

class ClipTool {
public:
    ClipTool() = default;
    ~ClipTool() = default;

    void EnablePlane(ClipPlaneAxis axis, double position);
    void DisablePlane();
    void SetPlanePosition(double position);
    double GetPlanePosition() const;

    void EnableBox(const spatial::BoundingBox& box);
    void DisableBox();
    void SetBoxBounds(const spatial::BoundingBox& box);

    bool IsPlaneEnabled() const { return plane_.enabled; }
    bool IsBoxEnabled() const { return box_.enabled; }
    const ClipPlane& GetPlane() const { return plane_; }
    const ClipBox& GetBox() const { return box_; }

    bool IsPointClipped(const math::Point3d& point) const;
    bool IsNodeClipped(const math::Point3d& nodeCenter, double nodeRadius) const;

    void SetFlipDirection(bool flip) { flip_ = flip; }
    bool GetFlipDirection() const { return flip_; }

    void RenderUI();

private:
    ClipPlane plane_;
    ClipBox box_;
    bool flip_ = false;

    math::Point3d ComputePlaneNormal(ClipPlaneAxis axis) const;
};

} // namespace tools
} // namespace workstation
