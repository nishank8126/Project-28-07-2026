#include "workstation/pod/HandlerTree.h"
#include <algorithm>
#include <limits>

namespace workstation { namespace pod {

HandlerTree::HandlerTree() {
    sentinel_.color = NodeColor::Black;
    sentinel_.left = &sentinel_;
    sentinel_.right = &sentinel_;
    sentinel_.parent = &sentinel_;
    root_ = &sentinel_;
}

HandlerTree::~HandlerTree() {
    destroyTree(root_);
}

void HandlerTree::destroyTree(HandlerTreeNode* node) {
    if (node == &sentinel_) return;
    destroyTree(node->left);
    destroyTree(node->right);
    delete node;
}

// ---- Left rotation (FUN_18000C3B0 evidence) ----
void HandlerTree::rotateLeft(HandlerTreeNode* x) {
    HandlerTreeNode* y = x->right;
    x->right = y->left;
    if (y->left != &sentinel_) y->left->parent = x;
    y->parent = x->parent;
    if (x->parent == &sentinel_)
        root_ = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    y->left = x;
    x->parent = y;
}

// ---- Right rotation ----
void HandlerTree::rotateRight(HandlerTreeNode* x) {
    HandlerTreeNode* y = x->left;
    x->left = y->right;
    if (y->right != &sentinel_) y->right->parent = x;
    y->parent = x->parent;
    if (x->parent == &sentinel_)
        root_ = y;
    else if (x == x->parent->right)
        x->parent->right = y;
    else
        x->parent->left = y;
    y->right = x;
    x->parent = y;
}

// ---- Insert ----
void HandlerTree::insert(HandlerKey key, PodBlockHandler* handler) {
    HandlerTreeNode* newNode = new HandlerTreeNode;
    newNode->key = std::move(key);
    newNode->handler = handler;
    newNode->left = &sentinel_;
    newNode->right = &sentinel_;
    newNode->color = NodeColor::Red;

    // BST insertion.
    HandlerTreeNode* parent = &sentinel_;
    HandlerTreeNode* current = root_;
    while (current != &sentinel_) {
        parent = current;
        if (HandlerKey::KeyLess(newNode->key.data(), current->key.data()))
            current = current->left;
        else
            current = current->right; // duplicates go right
    }
    newNode->parent = parent;

    if (parent == &sentinel_) {
        root_ = newNode;
    } else if (HandlerKey::KeyLess(newNode->key.data(), parent->key.data())) {
        parent->left = newNode;
    } else {
        parent->right = newNode;
    }

    ++size_;
    insertFixup(newNode);
}

// ---- Insert fixup (red-black repair) ----
void HandlerTree::insertFixup(HandlerTreeNode* z) {
    while (z->parent->color == NodeColor::Red) {
        if (z->parent == z->parent->parent->left) {
            HandlerTreeNode* y = z->parent->parent->right;
            if (y->color == NodeColor::Red) {
                // Case 1: uncle is red.
                z->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                z->parent->parent->color = NodeColor::Red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    // Case 2: uncle is black, z is right child.
                    z = z->parent;
                    rotateLeft(z);
                }
                // Case 3: uncle is black, z is left child.
                z->parent->color = NodeColor::Black;
                z->parent->parent->color = NodeColor::Red;
                rotateRight(z->parent->parent);
            }
        } else {
            // Mirror cases.
            HandlerTreeNode* y = z->parent->parent->left;
            if (y->color == NodeColor::Red) {
                z->parent->color = NodeColor::Black;
                y->color = NodeColor::Black;
                z->parent->parent->color = NodeColor::Red;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotateRight(z);
                }
                z->parent->color = NodeColor::Black;
                z->parent->parent->color = NodeColor::Red;
                rotateLeft(z->parent->parent);
            }
        }
    }
    root_->color = NodeColor::Black;
}

// ---- Lower-bound search (FUN_180077400 evidence) ----
const HandlerTreeNode* HandlerTree::lowerBoundNode(
    std::span<const std::uint8_t> key) const {
    const HandlerTreeNode* current = root_;
    const HandlerTreeNode* result = &sentinel_;
    while (current != &sentinel_) {
        if (!HandlerKey::KeyLess(current->key.data(), key)) {
            // current->key >= key
            result = current;
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return result;
}

PodBlockHandler* HandlerTree::lowerBound(
    std::span<const std::uint8_t> key) const {
    const HandlerTreeNode* node = lowerBoundNode(key);
    return (node != &sentinel_) ? node->handler : nullptr;
}

// ---- Validation: BST ordering ----
bool HandlerTree::validateBST(const HandlerTreeNode* node,
                              std::span<const std::uint8_t> lo,
                              std::span<const std::uint8_t> hi) const {
    if (node == &sentinel_) return true;
    if (!lo.empty() && !HandlerKey::KeyLess(lo, node->key.data())) return false;
    if (!hi.empty() && !HandlerKey::KeyLess(node->key.data(), hi)) return false;
    return validateBST(node->left, lo, node->key.data())
        && validateBST(node->right, node->key.data(), hi);
}

// ---- Validation: black-height ----
int HandlerTree::blackHeight(const HandlerTreeNode* node) const {
    if (node == &sentinel_) return 0;
    int lh = blackHeight(node->left);
    int rh = blackHeight(node->right);
    if (lh == -1 || rh == -1 || lh != rh) return -1;
    return lh + (node->color == NodeColor::Black ? 1 : 0);
}

// ---- Validation: red-black invariants ----
bool HandlerTree::validateRedBlack(const HandlerTreeNode* node, int& bh) const {
    if (node == &sentinel_) { bh = 0; return true; }
    if (node->color == NodeColor::Red) {
        if (node->left->color == NodeColor::Red) return false;
        if (node->right->color == NodeColor::Red) return false;
    }
    int lbh, rbh;
    if (!validateRedBlack(node->left, lbh)) return false;
    if (!validateRedBlack(node->right, rbh)) return false;
    if (lbh != rbh) return false;
    bh = lbh + (node->color == NodeColor::Black ? 1 : 0);
    return true;
}

bool HandlerTree::validateInvariants() const {
    if (root_ == &sentinel_) return true;
    if (root_->color != NodeColor::Black) return false;
    if (!validateBST(root_, {}, {})) return false;
    int bh = 0;
    return validateRedBlack(root_, bh);
}

bool HandlerTree::isRedBlackConsistent() const {
    return validateInvariants();
}

}} // namespace workstation::pod
