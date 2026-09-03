#include "workstation/spatial/CoordinateNormalizationManager.h"

namespace workstation { namespace spatial {

void CoordinateNormalizationManager::SetOrigin(double x, double y, double z) {
    originX_ = x;
    originY_ = y;
    originZ_ = z;
    normalized_ = true;
}

void CoordinateNormalizationManager::GetOrigin(double& x, double& y, double& z) const {
    x = originX_;
    y = originY_;
    z = originZ_;
}

void CoordinateNormalizationManager::RecomputeFromBoundingBox(const BoundingBox& combinedBounds) {
    originX_ = (combinedBounds.minX + combinedBounds.maxX) * 0.5;
    originY_ = (combinedBounds.minY + combinedBounds.maxY) * 0.5;
    originZ_ = (combinedBounds.minZ + combinedBounds.maxZ) * 0.5;
    normalized_ = true;
}

void CoordinateNormalizationManager::Reset() {
    originX_ = 0.0;
    originY_ = 0.0;
    originZ_ = 0.0;
    normalized_ = false;
}

void CoordinateNormalizationManager::WorldToLocal(double wx, double wy, double wz,
                                                  float& lx, float& ly, float& lz) const {
    lx = static_cast<float>(wx - originX_);
    ly = static_cast<float>(wy - originY_);
    lz = static_cast<float>(wz - originZ_);
}

void CoordinateNormalizationManager::LocalToWorld(float lx, float ly, float lz,
                                                  double& wx, double& wy, double& wz) const {
    wx = static_cast<double>(lx) + originX_;
    wy = static_cast<double>(ly) + originY_;
    wz = static_cast<double>(lz) + originZ_;
}

BoundingBox CoordinateNormalizationManager::WorldToLocal(const BoundingBox& world) const {
    BoundingBox local;
    local.minX = world.minX - originX_;
    local.minY = world.minY - originY_;
    local.minZ = world.minZ - originZ_;
    local.maxX = world.maxX - originX_;
    local.maxY = world.maxY - originY_;
    local.maxZ = world.maxZ - originZ_;
    return local;
}

BoundingBox CoordinateNormalizationManager::LocalToWorld(const BoundingBox& local) const {
    BoundingBox world;
    world.minX = local.minX + originX_;
    world.minY = local.minY + originY_;
    world.minZ = local.minZ + originZ_;
    world.maxX = local.maxX + originX_;
    world.maxY = local.maxY + originY_;
    world.maxZ = local.maxZ + originZ_;
    return world;
}

} // namespace spatial
} // namespace workstation
