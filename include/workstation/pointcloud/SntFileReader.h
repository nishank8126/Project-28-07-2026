#pragma once
#include "workstation/pointcloud/BoundingBox.h"

#include <cstdint>
#include <string>
#include <vector>

namespace workstation {
namespace pointcloud {

// A single 2-D/3-D polyline (or closed shape/ring) extracted from a .snt
// file. `points` holds count*3 world-space float32 XYZ triples.
struct SntPolyline {
    std::vector<float> points;
    size_t VertexCount() const { return points.size() / 3; }
};

struct SntEntities {
    std::vector<SntPolyline> polylines;
    BoundingBox bounds;
    uint32_t headerEntityCount = 0;  // total entities the file's header claims (Shapes+Text+LineStrings)
};

// Loads a .snt file (a proprietary 2-D CAD vector format -- Shapes/Text/
// LineStrings converted from DGN by the "NakshaApp" tool -- NOT a point
// cloud). No public specification exists for this format; the header and
// string pool were confirmed byte-for-byte against a companion conversion
// report, and the polyline/shape vertex encoding was confirmed by matching
// exact coordinates against a DXF export of the same drawing. This is a
// best-effort reader:
//   - Header, bounding box, and the polyline/shape vertex geometry are
//     decoded with high confidence.
//   - Circles and text labels are NOT decoded (their binary encoding wasn't
//     confirmed) -- callers should not expect them to appear.
// Returns false (with *errorMessage set) if the file isn't a valid .snt
// file at all (bad magic, truncated header, etc.); a structurally valid
// file with no recognizable polyline runs returns true with an empty
// `outEntities.polylines`.
bool LoadSntFile(const std::string& filepath, SntEntities& outEntities,
                  std::string* errorMessage = nullptr);

} // namespace pointcloud
} // namespace workstation
