#include "workstation/surface/SurfaceMesh.h"
#include <algorithm>
#include <unordered_set>

namespace workstation {
namespace surface {

void SurfaceMesh::Clear() {
    vertices_.clear();
    triangles_.clear();
    edgeIndices_.clear();
    bounds_ = {};
    name_.clear();
    sourceCloudID_ = 0;
}

std::vector<uint32_t> SurfaceMesh::GetIndexBuffer() const {
    std::vector<uint32_t> indices;
    indices.reserve(triangles_.size() * 3);
    for (const auto& tri : triangles_) {
        indices.push_back(tri.indices[0]);
        indices.push_back(tri.indices[1]);
        indices.push_back(tri.indices[2]);
    }
    return indices;
}

void SurfaceMesh::ComputeEdges() {
    edgeIndices_.clear();
    if (triangles_.empty()) return;

    // Deduplicate edges by a canonical 64-bit key (min<<32 | max) so interior
    // edges are drawn exactly once in wireframe mode. O(T) total.
    std::unordered_set<uint64_t> seen;
    seen.reserve(triangles_.size() * 3);
    edgeIndices_.reserve(triangles_.size() * 6); // upper bound before dedup

    for (const auto& tri : triangles_) {
        for (int e = 0; e < 3; ++e) {
            uint32_t v0 = tri.indices[e];
            uint32_t v1 = tri.indices[(e + 1) % 3];
            uint32_t a = (v0 < v1) ? v0 : v1;
            uint32_t b = (v0 < v1) ? v1 : v0;
            uint64_t key = (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
            if (seen.insert(key).second) {
                edgeIndices_.push_back(a);
                edgeIndices_.push_back(b);
            }
        }
    }
}

void SurfaceMesh::ComputeBounds() {
    if (vertices_.empty()) {
        bounds_ = {};
        return;
    }
    bounds_.minX = bounds_.minY = bounds_.minZ = std::numeric_limits<double>::max();
    bounds_.maxX = bounds_.maxY = bounds_.maxZ = std::numeric_limits<double>::lowest();

    for (const auto& v : vertices_) {
        bounds_.minX = std::min(bounds_.minX, static_cast<double>(v.position[0]));
        bounds_.minY = std::min(bounds_.minY, static_cast<double>(v.position[1]));
        bounds_.minZ = std::min(bounds_.minZ, static_cast<double>(v.position[2]));
        bounds_.maxX = std::max(bounds_.maxX, static_cast<double>(v.position[0]));
        bounds_.maxY = std::max(bounds_.maxY, static_cast<double>(v.position[1]));
        bounds_.maxZ = std::max(bounds_.maxZ, static_cast<double>(v.position[2]));
    }
}

} // namespace surface
} // namespace workstation
