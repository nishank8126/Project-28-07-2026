#pragma once
#include <optional>
#include <cstdint>

namespace workstation {

// Clean, typed replacement for the proprietary ElemMatSymb / OvrMatSymb
// bitmask layout (RE unknown: the exact mask bits 0x01..0x80000000 and the
// OvrMatSymb field layout are NOT reproduced). For Piece 3 it only needs to
// prove the flow:
//   original ViewContext state -> temporary modified state -> output
//   activation -> retained submission -> original state restoration.
struct ElementRenderOverrides {
    std::optional<uint64_t> lineColor;
    std::optional<uint64_t> fillColor;
    std::optional<uint64_t> materialToken;
    std::optional<uint64_t> auxiliaryValue;

    bool operator==(const ElementRenderOverrides& o) const {
        return lineColor == o.lineColor &&
               fillColor == o.fillColor &&
               materialToken == o.materialToken &&
               auxiliaryValue == o.auxiliaryValue;
    }
    bool operator!=(const ElementRenderOverrides& o) const {
        return !(*this == o);
    }
};

} // namespace workstation
