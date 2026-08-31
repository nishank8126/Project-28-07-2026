#pragma once
#include <cstdint>

namespace workstation {

// RE CONFIRMED: retained graphics submission accepts optional clip state
// (ClipPlaneSet*). The clip-plane structure/algorithms have NOT yet been
// reverse engineered enough to define our final clipping subsystem, so for now
// this remains an opaque/replaceable token. ClipVolumeToken is kept until the
// clipping subsystem is reverse engineered (Piece 18 / later).
//
// TransformToken was REMOVED in Piece 4A: the transform state is now the real
// verified workstation math type workstation::math::Transform3d. There is no
// longer a parallel token type.
struct ClipVolumeToken {
    uint64_t value = 0;
    bool operator==(const ClipVolumeToken& o) const { return value == o.value; }
    bool operator!=(const ClipVolumeToken& o) const { return !(*this == o); }
};

} // namespace workstation
