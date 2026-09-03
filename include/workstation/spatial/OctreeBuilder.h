#pragma once
#include "workstation/spatial/Octree.h"
#include <cstdint>
#include <vector>

namespace workstation { namespace spatial {

// Builds an Octree from an array of 3-D point positions.
//
// Usage:
//   OctreeBuilder builder;
//   builder.Build(positions, pointCount, maxDepth, maxLeafPoints);
//   auto root = builder.GetRoot();
//
class OctreeBuilder {
public:
    // Build the octree.  positions is an interleaved float3 array (x,y,z,x,y,z,...).
    // Returns the root node (owned by the builder).
    void Build(const float* positions, uint32_t pointCount,
               int maxDepth = OctreeNode::kMaxDepth,
               int maxLeafPoints = OctreeNode::kMaxLeafPoints);

    // Build from a subset of points indexed by an index array.
    void BuildIndexed(const float* positions, const uint32_t* indices,
                      uint32_t pointCount, int maxDepth = OctreeNode::kMaxDepth,
                      int maxLeafPoints = OctreeNode::kMaxLeafPoints);

    OctreeNode* GetRoot() const { return root_.get(); }
    std::unique_ptr<OctreeNode> ReleaseRoot() { return std::move(root_); }

    // Statistics
    uint32_t GetNodeCount() const { return nodeCount_; }
    uint32_t GetLeafCount() const { return leafCount_; }
    uint32_t GetMaxDepthReached() const { return maxDepthReached_; }

private:
    std::unique_ptr<OctreeNode> root_;
    uint32_t nodeCount_ = 0;
    uint32_t leafCount_ = 0;
    uint32_t maxDepthReached_ = 0;

    void Subdivide(OctreeNode& node, const float* positions,
                   std::vector<uint32_t>& indices,
                   int maxDepth, int maxLeafPoints);
};

} // namespace spatial
} // namespace workstation
