#pragma once
#include "workstation/pod/HandlerKey.h"
#include <cstddef>
#include <memory>

namespace workstation { namespace pod {

class PodBlockHandler;

enum class NodeColor { Red, Black };

// Red-black tree node for handler registry (FUN_18000C3B0 evidence).
// Clean-room implementation; no proprietary layout reproduced.
struct HandlerTreeNode {
    HandlerKey key;
    PodBlockHandler* handler = nullptr;

    HandlerTreeNode* parent = nullptr;
    HandlerTreeNode* left = nullptr;
    HandlerTreeNode* right = nullptr;
    NodeColor color = NodeColor::Red;
};

// Sentinel-based red-black tree for ordered handler storage.
// Sentinel node is black with no payload; used as NIL marker.
class HandlerTree {
public:
    HandlerTree();
    ~HandlerTree();

    HandlerTree(const HandlerTree&) = delete;
    HandlerTree& operator=(const HandlerTree&) = delete;

    // Insert a handler with the given key. Does NOT transfer ownership.
    void insert(HandlerKey key, PodBlockHandler* handler);

    // Find the handler whose key is the lower-bound for the search key.
    // Returns nullptr if no handler qualifies.
    PodBlockHandler* lowerBound(std::span<const std::uint8_t> key) const;

    bool empty() const { return size_ == 0; }
    std::size_t size() const { return size_; }

    // Validation helpers (for tests).
    bool validateInvariants() const;
    bool isRedBlackConsistent() const;

private:
    const HandlerTreeNode* lowerBoundNode(std::span<const std::uint8_t> key) const;

    void insertFixup(HandlerTreeNode* node);
    void rotateLeft(HandlerTreeNode* node);
    void rotateRight(HandlerTreeNode* node);
    void destroyTree(HandlerTreeNode* node);

    int blackHeight(const HandlerTreeNode* node) const;
    bool validateBST(const HandlerTreeNode* node,
                     std::span<const std::uint8_t> lo,
                     std::span<const std::uint8_t> hi) const;
    bool validateRedBlack(const HandlerTreeNode* node, int& bh) const;

    // Sentinel / NIL node (always black, no payload).
    HandlerTreeNode sentinel_;
    HandlerTreeNode* root_ = &sentinel_;
    std::size_t size_ = 0;
};

}} // namespace workstation::pod
