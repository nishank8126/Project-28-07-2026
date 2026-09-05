#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/spatial/BoundingBox.h"

#include <vector>
#include <cstdint>
#include <string>
#include <limits>

namespace workstation {
namespace surface {

struct SurfaceVertex {
    float position[3];
    float normal[3];
    float color[3];
    uint32_t classificationID;
};
static_assert(sizeof(SurfaceVertex) == 40, "SurfaceVertex must be 40 bytes (3*4 + 3*4 + 3*4 + 4)");

struct SurfaceTriangle {
    uint32_t indices[3];
};

class SurfaceMesh {
public:
    SurfaceMesh() = default;
    ~SurfaceMesh() = default;

    void Clear();
    bool IsEmpty() const { return vertices_.empty(); }

    uint32_t VertexCount() const { return static_cast<uint32_t>(vertices_.size()); }
    uint32_t TriangleCount() const { return static_cast<uint32_t>(triangles_.size()); }
    uint32_t IndexCount() const { return static_cast<uint32_t>(triangles_.size() * 3); }
    uint32_t EdgeCount() const { return static_cast<uint32_t>(edgeIndices_.size()); }

    std::vector<SurfaceVertex>& Vertices() { return vertices_; }
    const std::vector<SurfaceVertex>& Vertices() const { return vertices_; }

    std::vector<SurfaceTriangle>& Triangles() { return triangles_; }
    const std::vector<SurfaceTriangle>& Triangles() const { return triangles_; }

    std::vector<uint32_t> GetIndexBuffer() const;
    const std::vector<uint32_t>& GetEdgeIndexBuffer() const { return edgeIndices_; }

    // Collects the deduplicated set of unique triangle edges (used by the
    // wireframe pass). Edges are emitted as vertex-index pairs.
    void ComputeEdges();

    void SetBounds(const spatial::BoundingBox& b) { bounds_ = b; }
    const spatial::BoundingBox& GetBounds() const { return bounds_; }

    void SetName(const std::string& name) { name_ = name; }
    const std::string& GetName() const { return name_; }

    void SetSourceCloudID(uint32_t id) { sourceCloudID_ = id; }
    uint32_t GetSourceCloudID() const { return sourceCloudID_; }

    size_t GetGPUSizeEstimate() const {
        return vertices_.size() * sizeof(SurfaceVertex) +
               triangles_.size() * sizeof(SurfaceTriangle) +
               edgeIndices_.size() * sizeof(uint32_t);
    }

    void ComputeBounds();

private:
    std::vector<SurfaceVertex> vertices_;
    std::vector<SurfaceTriangle> triangles_;
    // Unique edges as {v0,v1,v2,v3,...} index pairs (2 per edge).
    std::vector<uint32_t> edgeIndices_;
    spatial::BoundingBox bounds_ = {};
    std::string name_;
    uint32_t sourceCloudID_ = 0;
};

} // namespace surface
} // namespace workstation
