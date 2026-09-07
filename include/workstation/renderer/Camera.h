#pragma once
#include "workstation/math/Matrix4d.h"
#include "workstation/math/Point3d.h"
#include "workstation/spatial/BoundingBox.h"

#include <cmath>
#include <algorithm>

namespace workstation {
namespace renderer {

enum class CameraProjection { Perspective, Orthographic };

struct FrustumPlanes {
    double left[4] = {};
    double right[4] = {};
    double top[4] = {};
    double bottom[4] = {};
    double nearPlane[4] = {};
    double farPlane[4] = {};

    void ExtractFromVP(const math::Matrix4d& vp);
    bool TestAABB(const spatial::BoundingBox& box) const;
};

class Camera {
public:
    Camera();

    void SetPerspective(double fovYDegrees, double aspectRatio,
                        double nearClip, double farClip);
    void SetAspectRatio(double aspectRatio) { aspectRatio_ = aspectRatio; dirty_ = true; }
    void SetOrthographic(double left, double right, double bottom, double top,
                         double nearClip, double farClip);
    void SetLookAt(const math::Point3d& eye, const math::Point3d& center,
                   const math::Point3d& up);

    void MoveForward(double distance);
    void MoveRight(double distance);
    void MoveUp(double distance);
    void Rotate(double yawDegrees, double pitchDegrees);
        void Zoom(double factor);
    void Pan(double dx, double dy);

    const math::Matrix4d& GetViewMatrix() const;
    const math::Matrix4d& GetProjectionMatrix() const;
    const math::Matrix4d& GetViewProjectionMatrix() const;
    const FrustumPlanes& GetFrustumPlanes() const;

    math::Point3d GetPosition() const { return position_; }
    math::Point3d GetTarget() const { return target_; }
    math::Point3d GetForward() const;
    math::Point3d GetRight() const;
    math::Point3d GetUp() const;

    double GetFOV() const { return fovY_; }
    double GetFovY() const { return fovY_; }
    double GetAspectRatio() const { return aspectRatio_; }
    double GetNearClip() const { return nearClip_; }
    double GetFarClip() const { return farClip_; }
    CameraProjection GetProjectionType() const { return projectionType_; }
    math::Point3d GetWorldUp() const { return worldUp_; }
    double GetYaw() const { return yaw_; }
    double GetPitch() const { return pitch_; }
    double GetOrthoLeft() const { return orthoLeft_; }
    double GetOrthoRight() const { return orthoRight_; }
    double GetOrthoBottom() const { return orthoBottom_; }
    double GetOrthoTop() const { return orthoTop_; }

    void Invalidate() { dirty_ = true; }

    void FocusOnBounds(const spatial::BoundingBox& bounds, double padding = 1.5);

    // Top/Plan view: camera directly above, looking straight down (-Z),
    // orthographic projection fitted to XY footprint.
    void SetTopView(const spatial::BoundingBox& bounds, double padding = 1.1);

private:
    math::Point3d position_ = {0, 0, 5};
    math::Point3d target_ = {0, 0, 0};
    math::Point3d worldUp_ = {0, 0, 1}; // Z-up: matches LiDAR/CAD convention

    double yaw_ = -90.0;
    double pitch_ = 0.0;

    double fovY_ = 45.0;
    double aspectRatio_ = 16.0 / 9.0;
    double nearClip_ = 0.1;
    double farClip_ = 10000.0;
    double orthoLeft_ = -10, orthoRight_ = 10;
    double orthoBottom_ = -10, orthoTop_ = 10;

    CameraProjection projectionType_ = CameraProjection::Perspective;

    mutable math::Matrix4d viewMatrix_;
    mutable math::Matrix4d projectionMatrix_;
    mutable math::Matrix4d viewProjectionMatrix_;
    mutable FrustumPlanes frustumPlanes_;
    mutable bool dirty_ = true;

    void UpdateMatrices() const;
    math::Matrix4d ComputeViewMatrix() const;
    math::Matrix4d ComputeProjectionMatrix() const;
};

} // namespace renderer
} // namespace workstation
