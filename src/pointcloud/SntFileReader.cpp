#include "workstation/pointcloud/SntFileReader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace workstation {
namespace pointcloud {

namespace {

uint32_t ReadU32(const std::vector<uint8_t>& buf, size_t offset) {
    uint32_t v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
}

double ReadF64(const std::vector<uint8_t>& buf, size_t offset) {
    double v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
}

float ReadF32(const std::vector<uint8_t>& buf, size_t offset) {
    float v;
    std::memcpy(&v, buf.data() + offset, sizeof(v));
    return v;
}

} // namespace

bool LoadSntFile(const std::string& filepath, SntEntities& outEntities,
                  std::string* errorMessage) {
    outEntities = SntEntities{};

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        if (errorMessage) *errorMessage = "failed to open .snt file";
        return false;
    }
    std::streamsize size = file.tellg();
    if (size < 0x60) {
        if (errorMessage) *errorMessage = ".snt file too small to contain a valid header";
        return false;
    }
    file.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buf.data()), size)) {
        if (errorMessage) *errorMessage = "failed to read .snt file";
        return false;
    }

    // ---- Header (confirmed byte-for-byte against a known-good sample; see
    // SntFileReader.h for the reverse-engineering caveats) ----
    if (std::memcmp(buf.data(), "SNT ", 4) != 0) {
        if (errorMessage) *errorMessage = "not a .snt file (bad magic)";
        return false;
    }

    outEntities.headerEntityCount = ReadU32(buf, 0x08);
    double bboxMinX = ReadF64(buf, 0x10);
    double bboxMinY = ReadF64(buf, 0x18);
    double bboxMaxX = ReadF64(buf, 0x20);
    double bboxMaxY = ReadF64(buf, 0x28);
    uint32_t entityTableOffset = ReadU32(buf, 0x50);
    uint32_t entityTableEnd = ReadU32(buf, 0x58);

    outEntities.bounds.minX = bboxMinX;
    outEntities.bounds.minY = bboxMinY;
    outEntities.bounds.maxX = bboxMaxX;
    outEntities.bounds.maxY = bboxMaxY;
    outEntities.bounds.minZ = 0.0;
    outEntities.bounds.maxZ = 0.0;

    if (entityTableOffset == 0 || entityTableEnd <= entityTableOffset ||
        entityTableEnd > buf.size()) {
        if (errorMessage) *errorMessage = "'.snt' header entity-table offsets look invalid";
        return false;
    }

    // ---- Geometry (polyline/shape vertex runs) ----
    // The entity record layout as a whole is NOT fully decoded (see header
    // comment), but the specific sub-pattern
    //   uint32 vertexCount; vertexCount * (float32 x, float32 y, float32 z)
    // was confirmed exactly against a DXF export of the same drawing (a
    // rectangle's 5 vertices -- 4 corners plus the closing repeat -- matched
    // byte-for-byte). This scans the entity table for that pattern and
    // treats every match as a polyline/shape ring; anything else in the
    // table (circles, text, per-entity type/layer/color fields) is skipped
    // over rather than misinterpreted.
    constexpr uint32_t kMinVertices = 2;
    constexpr uint32_t kMaxVertices = 100000;
    // Coordinates are allowed a little slack beyond the header bbox since
    // that bbox may itself be approximate for some source files.
    double marginX = (bboxMaxX - bboxMinX) * 0.05 + 1.0;
    double marginY = (bboxMaxY - bboxMinY) * 0.05 + 1.0;
    double loX = bboxMinX - marginX, hiX = bboxMaxX + marginX;
    double loY = bboxMinY - marginY, hiY = bboxMaxY + marginY;

    size_t pos = entityTableOffset;
    while (pos + 4 <= entityTableEnd) {
        uint32_t count = ReadU32(buf, pos);
        size_t vertexBytes = static_cast<size_t>(count) * 12;

        if (count >= kMinVertices && count <= kMaxVertices &&
            pos + 4 + vertexBytes <= entityTableEnd) {
            bool plausible = true;
            for (uint32_t v = 0; v < count && plausible; ++v) {
                float x = ReadF32(buf, pos + 4 + v * 12 + 0);
                float y = ReadF32(buf, pos + 4 + v * 12 + 4);
                if (x < loX || x > hiX || y < loY || y > hiY) plausible = false;
            }

            if (plausible) {
                SntPolyline poly;
                poly.points.resize(static_cast<size_t>(count) * 3);
                for (uint32_t v = 0; v < count; ++v) {
                    poly.points[v * 3 + 0] = ReadF32(buf, pos + 4 + v * 12 + 0);
                    poly.points[v * 3 + 1] = ReadF32(buf, pos + 4 + v * 12 + 4);
                    poly.points[v * 3 + 2] = ReadF32(buf, pos + 4 + v * 12 + 8);
                }
                outEntities.polylines.push_back(std::move(poly));
                pos += 4 + vertexBytes;
                continue;
            }
        }
        pos += 4;
    }

    fprintf(stderr, "[SntFileReader] %s: header reports %u entities; "
                     "recovered %zu polyline/shape geometries "
                     "(circles and text are not decoded by this reader)\n",
            filepath.c_str(), outEntities.headerEntityCount, outEntities.polylines.size());

    return true;
}

} // namespace pointcloud
} // namespace workstation
