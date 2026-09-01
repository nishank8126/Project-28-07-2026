#include "workstation/renderer/Camera.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace workstation {
namespace renderer {

Camera::Camera() {
    UpdateMatrices();
}

void Camera::SetPerspective(double fovYDegrees, double aspectRatio,
                             double nearClip, double farClip) {
    projectionType_ = CameraProjection::Perspective;
    fovY_ = fovYDegrees;
    aspectRatio_ = aspectRatio;
    nearClip_ = nearClip;
    farClip_ = farClip;
    dirty_ = true;
}

void Camera::SetOrthographic(double left, double right, double bottom, double top,
                              double nearClip, double farClip) {
    projectionType_ = CameraProjection::Orthographic;
    orthoLeft_ = left; orthoRight_ = right;
    orthoBottom_ = bottom; orthoTop_ = top;
    nearClip_ = nearClip;
    farClip_ = farClip;
    dirty_ = true;
}

void Camera::SetLookAt(const math::Point3d& eye, const math::Point3d& center,
                        const math::Point3d& up) {
    position_ = eye;
    target_ = center;
    worldUp_ = up;

    math::Point3d f = {center.x - eye.x, center.y - eye.y, center.z - eye.z};
    double len = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    if (len > 1e-10) { f.x /= len; f.y /= len; f.z /= len; }

    yaw_ = std::atan2(f.x, f.z) * 180.0 / M_PI;
    pitch_ = std::asin(f.y) * 180.0 / M_PI;

    dirty_ = true;
}

void Camera::MoveForward(double distance) {
    math::Point3d fwd = GetForward();
    position_.x += fwd.x * distance;
    position_.y += fwd.y * distance;
    position_.z += fwd.z * distance;
    dirty_ = true;
}

void Camera::MoveRight(double distance) {
    math::Point3d right = GetRight();
    position_.x += right.x * distance;
    position_.y += right.y * distance;
    position_.z += right.z * distance;
    dirty_ = true;
}

void Camera::MoveUp(double distance) {
    position_.y += distance;
    dirty_ = true;
}

void Camera::Rotate(double yawDegrees, double pitchDegrees) {
    yaw_ += yawDegrees;
    pitch_ += pitchDegrees;
    if (pitch_ > 89.0) pitch_ = 89.0;
    if (pitch_ < -89.0) pitch_ = -89.0;
    dirty_ = true;
}

void Camera::Zoom(double factor) {
    if (projectionType_ == CameraProjection::Perspective) {
        math::Point3d fwd = GetForward();
        position_.x += fwd.x * factor;
        position_.y += fwd.y * factor;
        position_.z += fwd.z * factor;
    } else {
        double scale = 1.0 - factor * 0.01;
        orthoLeft_ *= scale;
        orthoRight_ *= scale;
        orthoBottom_ *= scale;
        orthoTop_ *= scale;
    }
    dirty_ = true;
}

void Camera::Pan(double dx, double dy) {
    math::Point3d right = GetRight();
    math::Point3d up = GetUp();
    position_.x += right.x * dx + up.x * dy;
    position_.y += right.y * dx + up.y * dy;
    position_.z += right.z * dx + up.z * dy;
    dirty_ = true;
}

void Camera::FocusOnBounds(const spatial::BoundingBox& bounds, double padding) {
    double cx = (bounds.minX + bounds.maxX) * 0.5;
    double cy = (bounds.minY + bounds.maxY) * 0.5;
    double cz = (bounds.minZ + bounds.maxZ) * 0.5;

    double dx = (bounds.maxX - bounds.minX);
    double dy = (bounds.maxY - bounds.minY);
    double dz = (bounds.maxZ - bounds.minZ);
    double maxDim = std::max({dx, dy, dz});
    if (maxDim <= 0.0) maxDim = 1.0;

    double dist = maxDim * padding / std::tan(fovY_ * 0.5 * M_PI / 180.0);

    // SetLookAt() keeps yaw_/pitch_ (which ComputeViewMatrix() actually reads
    // via GetForward()) consistent with position_/target_; setting those two
    // directly here without it left the camera looking in a stale direction.
    SetLookAt({cx, cy, cz - dist}, {cx, cy, cz}, worldUp_);
}

math::Point3d Camera::GetForward() const {
    double yawRad = yaw_ * M_PI / 180.0;
    double pitchRad = pitch_ * M_PI / 180.0;
    return {
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::cos(yawRad)
    };
}

math::Point3d Camera::GetRight() const {
    math::Point3d fwd = GetForward();
    return {
        fwd.y * worldUp_.z - fwd.z * worldUp_.y,
        fwd.z * worldUp_.x - fwd.x * worldUp_.z,
        fwd.x * worldUp_.y - fwd.y * worldUp_.x
    };
}

math::Point3d Camera::GetUp() const {
    math::Point3d right = GetRight();
    math::Point3d fwd = GetForward();
    return {
        right.y * fwd.z - right.z * fwd.y,
        right.z * fwd.x - right.x * fwd.z,
        right.x * fwd.y - right.y * fwd.x
    };
}

const math::Matrix4d& Camera::GetViewMatrix() {
    if (dirty_) UpdateMatrices();
    return viewMatrix_;
}

const math::Matrix4d& Camera::GetProjectionMatrix() {
    if (dirty_) UpdateMatrices();
    return projectionMatrix_;
}

const math::Matrix4d& Camera::GetViewProjectionMatrix() {
    if (dirty_) UpdateMatrices();
    return viewProjectionMatrix_;
}

const FrustumPlanes& Camera::GetFrustumPlanes() {
    if (dirty_) UpdateMatrices();
    return frustumPlanes_;
}

void Camera::UpdateMatrices() {
    if (!dirty_) return;
    viewMatrix_ = ComputeViewMatrix();
    projectionMatrix_ = ComputeProjectionMatrix();
    viewProjectionMatrix_ = math::Matrix4d::Product(projectionMatrix_, viewMatrix_);
    frustumPlanes_.ExtractFromVP(viewProjectionMatrix_);
    dirty_ = false;
}

math::Matrix4d Camera::ComputeViewMatrix() {
    math::Point3d fwd = GetForward();
    math::Point3d right = GetRight();
    math::Point3d up = GetUp();

    return math::Matrix4d(
        right.x, right.y, right.z, -(right.x * position_.x + right.y * position_.y + right.z * position_.z),
        up.x, up.y, up.z, -(up.x * position_.x + up.y * position_.y + up.z * position_.z),
        -fwd.x, -fwd.y, -fwd.z, (fwd.x * position_.x + fwd.y * position_.y + fwd.z * position_.z),
        0, 0, 0, 1
    );
}

math::Matrix4d Camera::ComputeProjectionMatrix() {
    if (projectionType_ == CameraProjection::Orthographic) {
        double rl = orthoRight_ - orthoLeft_;
        double tb = orthoTop_ - orthoBottom_;
        double fn = farClip_ - nearClip_;
        return math::Matrix4d(
            2.0 / rl, 0, 0, -(orthoRight_ + orthoLeft_) / rl,
            0, 2.0 / tb, 0, -(orthoTop_ + orthoBottom_) / tb,
            0, 0, -2.0 / fn, -(farClip_ + nearClip_) / fn,
            0, 0, 0, 1
        );
    }

    double tanHalfFov = std::tan(fovY_ * 0.5 * M_PI / 180.0);
    return math::Matrix4d(
        1.0 / (aspectRatio_ * tanHalfFov), 0, 0, 0,
        0, -1.0 / tanHalfFov, 0, 0,
        0, 0, farClip_ / (nearClip_ - farClip_), (nearClip_ * farClip_) / (nearClip_ - farClip_),
        0, 0, -1, 0
    );
}

void FrustumPlanes::ExtractFromVP(const math::Matrix4d& vp) {
    for (int i = 0; i < 4; ++i) {
        left[i]   = vp(3, i) + vp(0, i);
        right[i]  = vp(3, i) - vp(0, i);
        bottom[i] = vp(3, i) + vp(1, i);
        top[i]    = vp(3, i) - vp(1, i);
        nearPlane[i] = vp(3, i) + vp(2, i);
        farPlane[i]  = vp(3, i) - vp(2, i);
    }

    auto normalize = [](double p[4]) {
        double len = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        if (len > 1e-10) { p[0] /= len; p[1] /= len; p[2] /= len; p[3] /= len; }
    };
    normalize(left); normalize(right);
    normalize(top); normalize(bottom);
    normalize(nearPlane); normalize(farPlane);
}

bool FrustumPlanes::TestAABB(const spatial::BoundingBox& box) const {
    auto testPlane = [&](const double plane[4]) {
        double x = (plane[0] > 0) ? box.maxX : box.minX;
        double y = (plane[1] > 0) ? box.maxY : box.minY;
        double z = (plane[2] > 0) ? box.maxZ : box.minZ;
        return (plane[0] * x + plane[1] * y + plane[2] * z + plane[3]) >= 0;
    };

    return testPlane(left) && testPlane(right) &&
           testPlane(top) && testPlane(bottom) &&
           testPlane(nearPlane) && testPlane(farPlane);
}

} // namespace renderer
} // namespace workstation
