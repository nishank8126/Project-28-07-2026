#pragma once
#include "workstation/pointcloud/BoundingBox.h"
#include <array>
#include <cstdint>
#include <vector>
#include <cstddef>

namespace workstation { namespace pod {

// Confirmed special geometry codes (FUN_180078840). Their semantic meaning
// is NOT yet proven; preserved as named constants per spec.
constexpr uint32_t kSpecialGeometryCodeA = 0x35F4C;
constexpr uint32_t kSpecialGeometryCodeB = 0x2748A;

// Geometry representation: two triplets of doubles (confirmed from float/double
// paths). No semantic meaning (min/max, center/extent, etc.) is assumed.
struct GeometryPair {
    double first[3] = {0.0, 0.0, 0.0};
    double second[3] = {0.0, 0.0, 0.0};
};

// Node type flags (FUN_1800764c0 / FUN_180081100).
enum class NodeType : uint32_t {
    Normal       = 0,
    Hierarchical = 1
};

// Geometry path selector.
enum class GeometryPath : uint32_t {
    Float32 = 0,   // 3 floats + 3 floats = 24 bytes → converted to doubles
    Float64 = 1    // 3 doubles + 3 doubles = 48 bytes
};

// Extra metadata (FUN_180082b40). The exact semantic meaning of each field
// is not yet proven.
struct NodeExtraMetadata {
    std::vector<uint8_t> rawSmallData;       // copied when param_2 <= 0x0C
    std::array<uint64_t, 6> extendedData{};  // copied when param_2 > 0x0C
    uint32_t mode = 0;                       // param_4
};

} // namespace pod
} // namespace workstation
