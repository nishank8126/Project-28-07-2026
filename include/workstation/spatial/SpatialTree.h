#pragma once
#include "workstation/spatial/SpatialNode.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace workstation { namespace spatial {

// Ordered hierarchical spatial index modelling the confirmed key-comparison
// behaviour of ptPtsToLoadInViewport:
//     if requestedKey < node.key  -> left
//     else                        -> right
// The concrete Bentley index (octree) is UNKNOWN; this binary-search tree
// implements exactly the confirmed traversal/accumulation behaviour.
class SpatialTree {
    SpatialNode* root_ = nullptr;
    size_t size_ = 0;

public:
    SpatialTree() = default;
    ~SpatialTree();

    SpatialTree(SpatialTree&& other) noexcept;
    SpatialTree& operator=(SpatialTree&& other) noexcept;
    SpatialTree(const SpatialTree&) = delete;
    SpatialTree& operator=(const SpatialTree&) = delete;

    void Clear();

    void Insert(uint64_t key, uint64_t pointCount, const BoundingBox& b = BoundingBox{});
    SpatialNode* Find(uint64_t key) const;

    // ptPtsToLoadInViewport: accumulate pointCount over the subtree rooted at
    // the matching entry (sum of all points in that spatial region).
    uint64_t Accumulate(uint64_t key) const;

    size_t Size() const { return size_; }

    // Visitor traversal. A small explicit stack is used; a production octree
    // would iterate cache-obliviously without per-traversal allocation.
    template <typename F>
    void Traverse(F f) const {
        if (!root_) return;
        std::vector<SpatialNode*> stack;
        stack.push_back(root_);
        while (!stack.empty()) {
            SpatialNode* n = stack.back();
            stack.pop_back();
            f(*n);
            if (n->left) stack.push_back(n->left);
            if (n->right) stack.push_back(n->right);
        }
    }

private:
    static void Insert(SpatialNode*& node, uint64_t key, uint64_t pc, const BoundingBox& b);
    static void Delete(SpatialNode* node);
};

} // namespace spatial
} // namespace workstation
