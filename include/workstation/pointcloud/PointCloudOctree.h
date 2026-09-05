#pragma once
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/spatial/Octree.h"
#include <cstdint>
#include <vector>
#include <array>

namespace workstation { namespace pointcloud {

// GPU-driven rendering data source for octree nodes.
// Provides per-node information needed by the GPU compute pipeline:
//   - Bounding box for frustum culling
//   - Point count for indirect draw command generation
//   - LOD level selected for the current frame
//   - GPU buffer reference for point data
//
// The octree is not owned by this class; it provides a read-only view
// of the existing octree structure built by OctreeBuilder.
struct PointCloudOctreeNode {
    uint64_t nodeKey;           // Unique identifier for GPU lookup
    BoundingBox bounds;         // Node axis-aligned bounding box
    uint32_t pointCount;        // Number of points in this node
    uint32_t lodLevel;          // Current LOD level (0 = full detail)
    bool visible;               // Frustum culling result
};

// Wrapper that extracts octree node data for GPU-driven rendering.
// The octree is assumed to be immutable during a render frame; the caller
// is responsible for synchronization if the octree is modified concurrently.
class PointCloudOctree {
public:
    PointCloudOctree() = default;
    ~PointCloudOctree() = default;

    // Initialize from an existing octree. Does not take ownership.
    void SetOctree(const spatial::OctreeNode* root, uint64_t rootKey) {
        root_ = root;
        rootKey_ = rootKey;
        BuildNodeMap();
    }

    // Number of nodes in the octree (including internal nodes).
    uint32_t NodeCount() const { return static_cast<uint32_t>(nodeKeys_.size()); }

    // Retrieve node data by index (0..NodeCount-1). Indices are stable
    // as long as the octree is not modified.
    const PointCloudOctreeNode& NodeAt(uint32_t idx) const {
        return nodes_[idx];
    }

    // Retrieve node data by key. Returns nullptr if not found.
    const PointCloudOctreeNode* NodeAtKey(uint64_t key) const {
        auto it = keyToNode_.find(key);
        if (it != keyToNode_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Screen-space LOD selection for a given node and camera.
    // Returns the recommended LOD level (0 = full detail, higher = simpler).
    // Camera parameters: position, view projection matrix, viewport dimensions, FOV.
    uint32_t SelectLOD(
        const float* cameraPos,
        const float* viewProj,
        uint32_t viewportWidth,
        uint32_t viewportHeight,
        float fovRadians) const;

    // Frustum culling: test a node's bounding box against 6 frustum planes.
    // planes are in column-major order (each 4 floats = one plane, w = distance).
    bool IsVisible(const float* planes) const;

    // Get the root node key for GPU command dispatch.
    uint64_t RootKey() const { return rootKey_; }

    // Get the total point count across all nodes.
    uint64_t TotalPointCount() const;

    // Get all node keys for GPU buffer mapping.
    const std::vector<uint64_t>& NodeKeys() const { return nodeKeys_; }

private:
    void BuildNodeMap();

    const spatial::OctreeNode* root_ = nullptr;
    uint64_t rootKey_ = 0;

    // Flattened node data for GPU access.
    std::vector<PointCloudOctreeNode> nodes_;
    std::vector<uint64_t> nodeKeys_;
    // Map from node key to index in nodes_ / nodeKeys_.
    std::unordered_map<uint64_t, size_t> keyToNode_;
};

inline uint32_t PointCloudOctree::SelectLOD(
    const float* cameraPos,
    const float* viewProj,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    float fovRadians) const
{
    if (!root_) return 0;

    // Simple screen-space error calculation:
    // SSE = (nodeRadius / distanceToCamera) * viewportHeight / tan(fov/2)
    // Map SSE to LOD level [0, maxLODLevels-1]

    const float kMaxSSE = 100.0f;  // pixels - above this = full detail (LOD 0)
    const float kMinSSE = 1.0f;    // pixels - below this = simplest LOD
    const int kMaxLODLevel = 4;    // LOD levels 0-4 (5 levels total)

    float totalError = 0.0f;
    int validNodes = 0;

    // Traverse all nodes and accumulate screen-space error
    for (uint32_t i = 0; i < NodeCount(); i++) {
        const auto& node = nodes_[i];

        // Approximate node center from bounds
        float cx = (float)(node.bounds.minX + node.bounds.maxX) * 0.5f;
        float cy = (float)(node.bounds.minY + node.bounds.maxY) * 0.5f;
        float cz = (float)(node.bounds.minZ + node.bounds.maxZ) * 0.5f;

        // Distance from camera to node center
        float dx = cx - cameraPos[0];
        float dy = cy - cameraPos[1];
        float dz = cz - cameraPos[2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 1.0f) dist = 1.0f;  // avoid divide-by-zero

        // Node radius (half diagonal)
        float rx = (float)(node.bounds.maxX - node.bounds.minX) * 0.5f;
        float ry = (float)(node.bounds.maxY - node.bounds.minY) * 0.5f;
        float rz = (float)(node.bounds.maxZ - node.bounds.minZ) * 0.5f;
        float nodeRadius = std::max({rx, ry, rz});

        // Screen-space error: angular size / pixel size approximation
        float sse = (nodeRadius / dist) * (viewportHeight / std::tan(fovRadians * 0.5f));

        // Accumulate (simple average for now)
        totalError += sse;
        validNodes++;
    }

    if (validNodes == 0) return 0;

    float avgSSE = totalError / float(validNodes);

    // Map SSE to LOD level: higher SSE -> lower LOD (more detail)
    // Clamp to [0, kMaxLODLevel]
    float normalized = (avgSSE - kMinSSE) / (kMaxSSE - kMinSSE);
    normalized = 1.0f - normalized;  // invert: high SSE -> low level number
    int lodLevel = std::max(0, std::min(kMaxLODLevel, int(normalized)));

    return static_cast<uint32_t>(lodLevel);
}

inline bool PointCloudOctree::IsVisible(const float* planes) const {
    if (!root_) return false;

    // Test all nodes against frustum planes
    for (uint32_t i = 0; i < NodeCount(); i++) {
        const auto& node = nodes_[i];

        // Quick bounding box vs 6-plane frustum test
        // For each plane: plane = (nx, ny, nz, d)
        // AABB is visible if not completely outside any plane
        bool outside = false;
        for (int p = 0; p < 6; p++) {
            float nx = planes[p * 4 + 0];
            float ny = planes[p * 4 + 1];
            float nz = planes[p * 4 + 2];
            float d = planes[p * 4 + 3];

            // Calculate AABB extents along plane normal
            float minX, maxX, minY, maxY, minZ, maxZ;
            if (nx >= 0.0f) {
                minX = node.bounds.minX; maxX = node.bounds.maxX;
            } else {
                minX = node.bounds.maxX; maxX = node.bounds.minX;
            }
            if (ny >= 0.0f) {
                minY = node.bounds.minY; maxY = node.bounds.maxY;
            } else {
                minY = node.bounds.maxY; maxY = node.bounds.minY;
            }
            if (nz >= 0.0f) {
                minZ = node.bounds.minZ; maxZ = node.bounds.maxZ;
            } else {
                minZ = node.bounds.maxZ; maxZ = node.bounds.minZ;
            }

            // Plane equation value at AABB
            float planeVal = nx * minX + ny * minY + nz * minZ + d;

            // If AABB is entirely on the negative side of plane, it's outside
            if (planeVal < 0.0f) {
                outside = true;
                break;
            }
        }
        // If not outside any plane, node is visible
        if (!outside) return true;
    }
    return false;
}

inline uint64_t PointCloudOctree::TotalPointCount() const {
    if (!root_) return 0;
    // Sum point counts from all leaf nodes
    uint64_t total = 0;
    // Traverse the octree recursively
    // Using a simple stack-based traversal
    std::vector<const spatial::OctreeNode*> stack;
    if (root_) stack.push_back(root_);

    while (!stack.empty()) {
        const spatial::OctreeNode* node = stack.back();
        stack.pop_back();

        if (node->IsEmpty()) continue;

        if (node->IsLeaf()) {
            total += node->pointCount;
        } else {
            for (int i = 0; i < 8; i++) {
                if (node->children[i]) {
                    stack.push_back(node->children[i].get());
                }
            }
        }
    }
    return total;
}

void PointCloudOctree::BuildNodeMap() {
    if (!root_) return;

    // Collect all nodes from the octree using BFS
    std::vector<const spatial::OctreeNode*> queue;
    if (root_) queue.push_back(root_);

    while (!queue.empty()) {
        const spatial::OctreeNode* node = queue.back();
        queue.pop_back();

        if (node->IsEmpty()) continue;

        // Create GPU-readable node data
        PointCloudOctreeNode gpuNode;
        gpuNode.nodeKey = reinterpret_cast<uint64_t>(node);  // use pointer as key
        gpuNode.bounds = node->bounds;
        gpuNode.pointCount = node->pointCount;
        gpuNode.lodLevel = 0;  // Will be set by LOD selection each frame
        gpuNode.visible = false;  // Will be set by frustum culling each frame

        size_t idx = nodes_.size();
        nodes_.push_back(gpuNode);
        nodeKeys_.push_back(gpuNode.nodeKey);
        keyToNode_[gpuNode.nodeKey] = idx;

        // Add children to queue
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                queue.push_back(node->children[i].get());
            }
        }
    }
}

}} // namespace workstation::pointcloud