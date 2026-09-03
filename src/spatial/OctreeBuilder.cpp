#include "workstation/spatial/OctreeBuilder.h"
#include <algorithm>
#include <numeric>

namespace workstation { namespace spatial {

void OctreeBuilder::Build(const float* positions, uint32_t pointCount,
                           int maxDepth, int maxLeafPoints) {
    std::vector<uint32_t> indices(pointCount);
    std::iota(indices.begin(), indices.end(), 0u);
    BuildIndexed(positions, indices.data(), pointCount, maxDepth, maxLeafPoints);
}

void OctreeBuilder::BuildIndexed(const float* positions, const uint32_t* inIndices,
                                  uint32_t pointCount, int maxDepth, int maxLeafPoints) {
    nodeCount_ = 0;
    leafCount_ = 0;
    maxDepthReached_ = 0;
    root_.reset();

    if (pointCount == 0) return;

    // Compute bounding box from point positions.
    float minX = positions[0], minY = positions[1], minZ = positions[2];
    float maxX = minX, maxY = minY, maxZ = minZ;
    for (uint32_t i = 1; i < pointCount; ++i) {
        float px = positions[i * 3 + 0];
        float py = positions[i * 3 + 1];
        float pz = positions[i * 3 + 2];
        minX = std::min(minX, px); maxX = std::max(maxX, px);
        minY = std::min(minY, py); maxY = std::max(maxY, py);
        minZ = std::min(minZ, pz); maxZ = std::max(maxZ, pz);
    }

    // Add small epsilon to avoid degenerate zero-width boxes.
    float eps = std::max({maxX - minX, maxY - minY, maxZ - minZ}) * 1e-6f;
    if (eps < 1e-6f) eps = 1e-6f;

    root_ = std::make_unique<OctreeNode>();
    root_->bounds.minX = static_cast<double>(minX) - eps;
    root_->bounds.minY = static_cast<double>(minY) - eps;
    root_->bounds.minZ = static_cast<double>(minZ) - eps;
    root_->bounds.maxX = static_cast<double>(maxX) + eps;
    root_->bounds.maxY = static_cast<double>(maxY) + eps;
    root_->bounds.maxZ = static_cast<double>(maxZ) + eps;
    root_->totalPoints = pointCount;
    root_->depth = 0;
    nodeCount_ = 1;

    // Copy index array so we can partition in-place.
    std::vector<uint32_t> indices(inIndices, inIndices + pointCount);

    Subdivide(*root_, positions, indices, maxDepth, maxLeafPoints);
}

void OctreeBuilder::Subdivide(OctreeNode& node, const float* positions,
                               std::vector<uint32_t>& indices,
                               int maxDepth, int maxLeafPoints) {
    if (node.totalPoints <= static_cast<uint64_t>(maxLeafPoints) ||
        static_cast<int>(node.depth) >= maxDepth) {
        // Leaf node: store point offset/count.
        // The caller must arrange for the indices in [offset, offset+count)
        // to correspond to the node's point range in the global array.
        // Since we operate on a contiguous sub-range via the index array,
        // we set offset=0 and let the caller copy/sort.
        node.pointOffset = 0;
        node.pointCount = static_cast<uint32_t>(node.totalPoints);
        node.pointCount = node.pointCount;  // store in leaf for draw range
        ++leafCount_;
        if (node.depth > maxDepthReached_) maxDepthReached_ = node.depth;
        return;
    }

    // Partition the indices into 8 octants.
    std::array<std::vector<uint32_t>, 8> octantIndices;
    for (uint32_t idx : indices) {
        double px = positions[idx * 3 + 0];
        double py = positions[idx * 3 + 1];
        double pz = positions[idx * 3 + 2];
        int octant = node.GetOctant(px, py, pz);
        octantIndices[octant].push_back(idx);
    }

    // Replace the caller's indices with the reordered sequence so that
    // points belonging to the same octant are contiguous.  This is used by
    // leaf nodes to have a contiguous draw range.
    indices.clear();
    for (int i = 0; i < 8; ++i) {
        for (uint32_t idx : octantIndices[i]) {
            indices.push_back(idx);
        }
    }

    // Create children for non-empty octants.
    for (int i = 0; i < 8; ++i) {
        if (octantIndices[i].empty()) continue;

        auto child = std::make_unique<OctreeNode>();
        child->bounds = node.GetOctantBounds(i);
        child->totalPoints = octantIndices[i].size();
        child->depth = node.depth + 1;
        ++nodeCount_;

        // Compute the sub-range of indices for this child.
        // Since we just appended them in order, the child's indices are
        // at the tail of the previous children and the head of the next.
        // We compute the offset by counting how many indices precede this
        // child in the reordered vector.
        size_t offset = 0;
        for (int j = 0; j < i; ++j) {
            offset += octantIndices[j].size();
        }
        std::vector<uint32_t> childIndices(indices.begin() + offset,
                                            indices.begin() + offset + octantIndices[i].size());

        // Recurse: childIndices will be reordered in-place.
        Subdivide(*child, positions, childIndices, maxDepth, maxLeafPoints);

        // Write back reordered indices.
        std::copy(childIndices.begin(), childIndices.end(), indices.begin() + offset);

        // If the child is a leaf, set its pointOffset to reflect its position
        // in the global indices array.
        if (child->IsLeaf()) {
            child->pointOffset = static_cast<uint32_t>(offset);
        }

        node.children[i] = std::move(child);
    }
}

} // namespace spatial
} // namespace workstation
