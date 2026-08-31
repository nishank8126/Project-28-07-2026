#pragma once
#include <cstdint>

namespace workstation {

// Placeholder for element display / symbology state captured around graphics
// generation. The proprietary ElemDisplayParams layout has NOT been
// reconstructed; these fields are OUR temporary implementation. The important
// Piece 2 requirement is copy / save / restore behavior (RE L/5).
struct ElemDisplayParams {
    uint32_t symbologyRevision = 0;
    uint32_t materialRevision = 0;

    bool operator==(const ElemDisplayParams& o) const {
        return symbologyRevision == o.symbologyRevision &&
               materialRevision == o.materialRevision;
    }
};

} // namespace workstation
