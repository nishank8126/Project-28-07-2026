#pragma once
#include "workstation/pod/HandlerTree.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace workstation { namespace pod {

// Forward declaration.
class PodDecodeContext;

// Abstract handler for POD block entries (FUN_180077BD0 evidence).
// Clean-room equivalent; no proprietary vtable reproduced.
class PodBlockHandler {
public:
    virtual ~PodBlockHandler() = default;

    // Process the handler on the given decode context.
    // Returns true on success, false on error.
    virtual bool process(PodDecodeContext& context) = 0;
};

// Ordered byte-key handler registry backed by a red-black tree (FUN_18000C3B0).
// Lower-bound search uses recovered byte comparison (FUN_180077400 / FUN_18000B940).
class PodHandlerRegistry {
public:
    using HandlerKey = std::vector<std::uint8_t>;

    // Insert a handler with the given key.
    void insert(HandlerKey key, PodBlockHandler* handler);

    // Find the handler whose key is the lower-bound for the search key.
    // Returns nullptr if no handler qualifies.
    PodBlockHandler* findLowerBound(std::span<const std::uint8_t> key) const;

    std::size_t size() const { return tree_.size(); }
    bool empty() const { return tree_.empty(); }

    // Validation helpers (for tests).
    bool validateInvariants() const { return tree_.validateInvariants(); }

private:
    HandlerTree tree_;
};

}} // namespace workstation::pod
