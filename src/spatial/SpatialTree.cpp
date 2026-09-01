#include "workstation/spatial/SpatialTree.h"

#include <vector>

namespace workstation { namespace spatial {

SpatialTree::~SpatialTree() { Delete(root_); }

SpatialTree::SpatialTree(SpatialTree&& other) noexcept
    : root_(other.root_), size_(other.size_) {
    other.root_ = nullptr;
    other.size_ = 0;
}

SpatialTree& SpatialTree::operator=(SpatialTree&& other) noexcept {
    if (this != &other) {
        Delete(root_);
        root_ = other.root_;
        size_ = other.size_;
        other.root_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void SpatialTree::Clear() {
    Delete(root_);
    root_ = nullptr;
    size_ = 0;
}

void SpatialTree::Insert(SpatialNode*& node, uint64_t key, uint64_t pc, const BoundingBox& b) {
    if (!node) {
        node = new SpatialNode();
        node->key = key;
        node->pointCount = pc;
        node->bounds = b;
        return;
    }
    if (key < node->key)
        Insert(node->left, key, pc, b);
    else
        Insert(node->right, key, pc, b);
}

void SpatialTree::Insert(uint64_t key, uint64_t pc, const BoundingBox& b) {
    Insert(root_, key, pc, b);
    ++size_;
}

SpatialNode* SpatialTree::Find(uint64_t key) const {
    SpatialNode* n = root_;
    while (n) {
        if (key == n->key) return n;
        n = (key < n->key) ? n->left : n->right;
    }
    return nullptr;
}

uint64_t SpatialTree::Accumulate(uint64_t key) const {
    const SpatialNode* n = Find(key);
    if (!n) return 0;
    uint64_t sum = 0;
    std::vector<const SpatialNode*> st;
    st.push_back(n);
    while (!st.empty()) {
        const SpatialNode* c = st.back();
        st.pop_back();
        sum += c->pointCount;
        if (c->left) st.push_back(c->left);
        if (c->right) st.push_back(c->right);
    }
    return sum;
}

void SpatialTree::Delete(SpatialNode* node) {
    if (!node) return;
    Delete(node->left);
    Delete(node->right);
    delete node;
}

} // namespace spatial
} // namespace workstation
