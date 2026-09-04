#include "workstation/cad/SntAttachment.h"
#include "workstation/cad/AciColorTable.h"

#include <fstream>
#include <cmath>
#include <cstring>

namespace workstation {
namespace cad {

static constexpr double kPi = 3.14159265358979323846;
static constexpr uint32_t kLegacyMagicV0 = 0x00544E53;
static constexpr uint32_t kLegacyMagicV1 = 0x01544E53;
static constexpr int kLegacyEtypePolyline = 0;
static constexpr int kLegacyEtypeText = 1;
static constexpr int kLegacyEtype3dface = 2;

static const std::array<uint8_t, 3> kLayerColorCycle[] = {
    {0, 255, 80}, {0, 220, 255}, {255, 220, 50},
    {255, 80, 100}, {200, 130, 255}, {255, 160, 60},
    {100, 255, 150},
};
static constexpr int kLayerColorCycleSize = 7;

SntAttachment::SntAttachment(const std::string& filepath)
    : m_filePath(filepath) {}

std::string SntAttachment::filename() const {
    auto pos = m_filePath.find_last_of("/\\");
    return (pos != std::string::npos) ? m_filePath.substr(pos + 1) : m_filePath;
}

bool SntAttachment::load(std::string* errorMessage) {
    std::ifstream file(m_filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        if (errorMessage) *errorMessage = "Cannot open file: " + m_filePath;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        if (errorMessage) *errorMessage = "Failed to read file";
        return false;
    }
    file.close();

    if (data.size() < 24) {
        if (errorMessage) *errorMessage = "File too small to be a valid SNT file";
        return false;
    }

    const uint8_t* raw = data.data();
    size_t dataSize = data.size();

    uint32_t magic = 0;
    std::memcpy(&magic, raw, 4);

    bool ok = false;
    if (magic == kLegacyMagicV0 || magic == kLegacyMagicV1) {
        uint16_t verMajor = 0, verMinor = 0;
        std::memcpy(&verMajor, raw + 4, 2);
        std::memcpy(&verMinor, raw + 6, 2);
        m_doc.versionMajor = verMajor;
        m_doc.versionMinor = verMinor;

        if (magic == kLegacyMagicV1) {
            ok = loadModern(raw, dataSize, errorMessage);
        } else {
            ok = loadLegacyV0(raw, dataSize, errorMessage);
        }
    } else {
        if (raw[0] == 'S' && raw[1] == 'N' && raw[2] == 'T' && raw[3] == ' ') {
            ok = loadModern(raw, dataSize, errorMessage);
        } else {
            if (errorMessage) *errorMessage = "Unrecognized SNT magic bytes";
            return false;
        }
    }

    if (!ok) return false;

    m_layerManager.clear();
    for (const auto& [name, color] : m_doc.layers) {
        int count = 0;
        for (const auto& ent : m_doc.entities) {
            if (ent.layer == name) ++count;
        }
        m_layerManager.addLayer(name, color[0], color[1], color[2], count);
    }

    m_loaded = true;
    return true;
}

bool SntAttachment::loadLegacyV0(const uint8_t* data, size_t size, std::string* errorMessage) {
    size_t pos = 8;
    std::memcpy(&m_doc.versionMajor, data + pos, 2); pos += 2;
    std::memcpy(&m_doc.versionMinor, data + pos, 2); pos += 2;
    pos += 2;

    uint32_t numStrings = 0, numLayers = 0, numEntities = 0;
    std::memcpy(&numStrings, data + pos, 4); pos += 4;
    std::memcpy(&numLayers, data + pos, 4); pos += 4;
    std::memcpy(&numEntities, data + pos, 4); pos += 4;

    m_doc.strings.resize(numStrings);
    for (uint32_t i = 0; i < numStrings; ++i) {
        if (pos + 4 > size) break;
        uint32_t slen = 0;
        std::memcpy(&slen, data + pos, 4); pos += 4;
        if (pos + slen > size) break;
        m_doc.strings[i] = std::string(reinterpret_cast<const char*>(data + pos), slen);
        pos += slen;
    }

    m_doc.layers.resize(numLayers);
    for (uint32_t i = 0; i < numLayers; ++i) {
        if (pos + 7 > size) break;
        uint32_t nameIdx = 0;
        std::memcpy(&nameIdx, data + pos, 4); pos += 4;
        uint8_t r = data[pos], g = data[pos + 1], b = data[pos + 2]; pos += 3;

        std::string name = (nameIdx < numStrings) ? m_doc.strings[nameIdx] : "Layer" + std::to_string(i);
        std::array<uint8_t, 3> color = ((r + g + b) > 0) ? std::array<uint8_t, 3>{r, g, b}
                                                          : kLayerColorCycle[i % kLayerColorCycleSize];
        m_doc.layers[i] = {name, color};
    }

    double minX = 1e30, minY = 1e30, maxX = -1e30, maxY = -1e30;
    bool anyBBox = false;

    for (uint32_t i = 0; i < numEntities; ++i) {
        if (pos >= size) break;
        uint8_t etype = data[pos]; pos += 1;
        uint32_t layerIdx = 0;
        std::memcpy(&layerIdx, data + pos, 4); pos += 4;

        std::string layerName = (layerIdx < numLayers) ? m_doc.layers[layerIdx].first : "0";
        std::array<uint8_t, 3> layerColor = (layerIdx < numLayers) ? m_doc.layers[layerIdx].second : std::array<uint8_t, 3>{255, 255, 255};

        SntEntity ent;
        ent.layer = layerName;
        ent.colorR = layerColor[0]; ent.colorG = layerColor[1]; ent.colorB = layerColor[2];

        if (etype == kLegacyEtypePolyline) {
            if (pos + 4 > size) break;
            uint32_t nv = 0;
            std::memcpy(&nv, data + pos, 4); pos += 4;
            ent.type = SntEntity::Polyline;
            ent.vertices.resize(nv);
            for (uint32_t v = 0; v < nv; ++v) {
                if (pos + 24 > size) break;
                double x, y, z;
                std::memcpy(&x, data + pos, 8); pos += 8;
                std::memcpy(&y, data + pos, 8); pos += 8;
                std::memcpy(&z, data + pos, 8); pos += 8;
                ent.vertices[v] = {x, y, z};
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
                anyBBox = true;
            }
            ent.closed = (nv >= 3);
            m_doc.entities.push_back(ent);
        }
        else if (etype == kLegacyEtypeText) {
            if (pos + 4 > size) break;
            uint32_t tlen = 0;
            std::memcpy(&tlen, data + pos, 4); pos += 4;
            if (pos + tlen > size) break;
            ent.type = SntEntity::Text;
            ent.text = std::string(reinterpret_cast<const char*>(data + pos), tlen);
            pos += tlen;
            if (pos + 24 > size) break;
            double x, y, z;
            std::memcpy(&x, data + pos, 8); pos += 8;
            std::memcpy(&y, data + pos, 8); pos += 8;
            std::memcpy(&z, data + pos, 8); pos += 8;
            ent.vertices.push_back({x, y, z});
            if (pos + 8 <= size) {
                std::memcpy(&ent.textHeight, data + pos, 8); pos += 8;
            }
            m_doc.entities.push_back(ent);
        }
        else if (etype == kLegacyEtype3dface) {
            ent.type = SntEntity::ThreeDFace;
            ent.vertices.resize(4);
            for (int v = 0; v < 4; ++v) {
                if (pos + 24 > size) break;
                double x, y, z;
                std::memcpy(&x, data + pos, 8); pos += 8;
                std::memcpy(&y, data + pos, 8); pos += 8;
                std::memcpy(&z, data + pos, 8); pos += 8;
                ent.vertices[v] = {x, y, z};
                minX = std::min(minX, x); maxX = std::max(maxX, x);
                minY = std::min(minY, y); maxY = std::max(maxY, y);
                anyBBox = true;
            }
            m_doc.entities.push_back(ent);
        }
        else {
            break;
        }
    }

    if (anyBBox) {
        m_doc.bboxMinX = minX; m_doc.bboxMinY = minY;
        m_doc.bboxMaxX = maxX; m_doc.bboxMaxY = maxY;
        m_doc.hasBBox = true;
    }

    return true;
}

namespace {
// -- Modern SNT binary format, reverse-engineered and byte-exact validated
// against real files against the DGN->SNT converter's own struct formats
// (Python's `struct` module notation kept in comments for direct cross-
// reference against that source):
//
//   header:  "<IbbHQ4d6QI28s"  = 128 bytes
//     magic(u32) verMajor(i8) verMinor(i8) flags(u16) entityCount(u64)
//     bboxMinX/MinY/MaxX/MaxY(4x double)
//     t[0..5] (6x u64): t0=metaOff, t1=metaEnd(=stringPoolOff),
//                       t2=stringPoolEnd(=layerTableOff), t3=layerTableEnd,
//                       t4=entityTableOff (duplicate of t3), t5=entityTableEnd
//     crc(u32) creator(28 bytes)
//
//   string pool [t1,t2): count(u32) then per string: len(u16) + utf8 bytes
//   layer table [t2,t3): records of "<IHHBBH" = 12 bytes each:
//     name_idx(u32) color_aci(u16) linetype_idx(u16) flags(u8) reserved(u8) pad(u16)
//   entity table [t4,t5): sequence of [17-byte header][body_size bytes body]
//     header "<BHBIIBI": type(u8) layer_idx(u16) color_mode(u8) color_value(u32)
//                        entity_id(u32, unused here) lineweight(u8) body_size(u32)
//
// Verified end-to-end against a real 464-entity file: every entity landed
// exactly on the declared table end with zero decode errors, and decoded
// TEXT/LWPOLYLINE bodies matched the file's actual grid-tile content.
constexpr size_t kModernHeaderSize = 128;
constexpr size_t kModernEntityHdrSize = 17;
constexpr size_t kModernLayerSize = 12;

enum ModernEntityType : uint8_t {
    kMETLine = 0x01,
    kMETLwPolyline = 0x02,
    kMETPolyline3D = 0x03,
    kMETArc = 0x04,
    kMETCircle = 0x05,
    kMETEllipse = 0x06,
    kMETSpline = 0x07,
    kMETPoint = 0x08,
    kMETText = 0x10,
    kMETMText = 0x11,
    kMETDimension = 0x12,
    kMETSolid = 0x21,
    kMETFace3d = 0x22,
    kMETMesh = 0x50,
};

enum ModernColorMode : uint8_t { kMCMByLayer = 0, kMCMAci = 1, kMCMRgb = 2 };

float ReadF32(const uint8_t* p) { float v; std::memcpy(&v, p, 4); return v; }
uint16_t ReadU16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
uint32_t ReadU32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint64_t ReadU64(const uint8_t* p) { uint64_t v; std::memcpy(&v, p, 8); return v; }
double ReadF64(const uint8_t* p) { double v; std::memcpy(&v, p, 8); return v; }

} // namespace

bool SntAttachment::loadModern(const uint8_t* data, size_t size, std::string* errorMessage) {
    if (size < kModernHeaderSize) {
        if (errorMessage) *errorMessage = "SNT file too small for modern header";
        return false;
    }

    if (std::memcmp(data, "SNT ", 4) != 0 && std::memcmp(data, "SNT\x01", 4) != 0) {
        if (errorMessage) *errorMessage = "Not a valid SNT file (bad magic)";
        return false;
    }

    m_doc.versionMajor = static_cast<int8_t>(data[4]);
    m_doc.versionMinor = static_cast<int8_t>(data[5]);
    // uint16_t flags = ReadU16(data + 6); // FLAG_HAS_INDEX / FLAG_HAS_CRS, unused here

    double bboxMinX = ReadF64(data + 16);
    double bboxMinY = ReadF64(data + 24);
    double bboxMaxX = ReadF64(data + 32);
    double bboxMaxY = ReadF64(data + 40);
    m_doc.bboxMinX = bboxMinX; m_doc.bboxMinY = bboxMinY;
    m_doc.bboxMaxX = bboxMaxX; m_doc.bboxMaxY = bboxMaxY;
    m_doc.hasBBox = true;

    uint64_t t[6];
    for (int i = 0; i < 6; ++i) t[i] = ReadU64(data + 48 + i * 8);
    uint64_t stringPoolOffset = t[1], stringPoolEnd = t[2];
    uint64_t layerTableOffset = t[2], layerTableEnd = t[3];
    uint64_t entityTableOffset = t[4], entityTableEnd = t[5];

    if (entityTableOffset == 0 || entityTableEnd < entityTableOffset || entityTableEnd > size ||
        layerTableEnd > size || stringPoolEnd > size) {
        if (errorMessage) *errorMessage = "Invalid modern SNT table offsets";
        return false;
    }

    // -- string pool --------------------------------------------------------
    std::vector<std::string> strings;
    {
        size_t sp = static_cast<size_t>(stringPoolOffset);
        if (sp + 4 <= stringPoolEnd) {
            uint32_t strCount = ReadU32(data + sp);
            sp += 4;
            strings.reserve(strCount);
            for (uint32_t i = 0; i < strCount && sp + 2 <= stringPoolEnd; ++i) {
                uint16_t slen = ReadU16(data + sp);
                sp += 2;
                if (sp + slen > stringPoolEnd) break;
                strings.emplace_back(reinterpret_cast<const char*>(data + sp), slen);
                sp += slen;
            }
        }
    }

    // -- layer table ----------------------------------------------------------
    m_doc.layers.clear();
    {
        size_t lp = static_cast<size_t>(layerTableOffset);
        while (lp + kModernLayerSize <= layerTableEnd) {
            uint32_t nameIdx = ReadU32(data + lp);
            uint16_t colorAci = ReadU16(data + lp + 4);
            std::string name = (nameIdx < strings.size()) ? strings[nameIdx]
                : ("Layer" + std::to_string(m_doc.layers.size()));
            auto color = resolveEntityColor(static_cast<int>(colorAci), -1);
            if (color[0] == 255 && color[1] == 255 && color[2] == 255) {
                color = kLayerColorCycle[m_doc.layers.size() % kLayerColorCycleSize];
            }
            m_doc.layers.push_back({name, color});
            lp += kModernLayerSize;
        }
    }

    // -- entity table -----------------------------------------------------------
    m_doc.entities.clear();
    size_t ep = static_cast<size_t>(entityTableOffset);
    while (ep + kModernEntityHdrSize <= entityTableEnd) {
        uint8_t etype = data[ep];
        uint16_t layerIdx = ReadU16(data + ep + 1);
        uint8_t colorMode = data[ep + 3];
        uint32_t colorValue = ReadU32(data + ep + 4);
        // entity_id at data+ep+8 (u32) is not needed for rendering.
        uint8_t lineweight = data[ep + 12];
        uint32_t bodySize = ReadU32(data + ep + 13);

        size_t bodyStart = ep + kModernEntityHdrSize;
        size_t bodyEnd = bodyStart + bodySize;
        if (bodyEnd > entityTableEnd) break; // corrupt tail - stop rather than misread

        std::string layerName = (layerIdx < m_doc.layers.size()) ? m_doc.layers[layerIdx].first : "0";
        std::array<uint8_t, 3> color;
        if (colorMode == kMCMRgb) {
            color = {static_cast<uint8_t>((colorValue >> 16) & 0xFF),
                     static_cast<uint8_t>((colorValue >> 8) & 0xFF),
                     static_cast<uint8_t>(colorValue & 0xFF)};
        } else if (colorMode == kMCMAci) {
            color = resolveEntityColor(static_cast<int>(colorValue & 0xFF), static_cast<int>(layerIdx));
        } else { // BYLAYER
            color = resolveEntityColor(-1, static_cast<int>(layerIdx));
        }

        const uint8_t* body = data + bodyStart;
        SntEntity ent;
        ent.layer = layerName;
        ent.colorR = color[0]; ent.colorG = color[1]; ent.colorB = color[2];
        ent.lineweight = lineweight;
        bool pushed = false;

        switch (etype) {
            case kMETLine:
                if (bodySize >= 24) {
                    ent.type = SntEntity::Polyline;
                    ent.vertices = {
                        {ReadF32(body + 0), ReadF32(body + 4), ReadF32(body + 8)},
                        {ReadF32(body + 12), ReadF32(body + 16), ReadF32(body + 20)},
                    };
                    ent.closed = false;
                    pushed = true;
                }
                break;
            case kMETLwPolyline: {
                if (bodySize >= 9) {
                    float elev = ReadF32(body + 0);
                    uint8_t flagsLw = body[4];
                    uint32_t vcount = ReadU32(body + 5);
                    if (static_cast<size_t>(9) + static_cast<size_t>(vcount) * 12 <= bodySize) {
                        ent.type = SntEntity::Polyline;
                        ent.vertices.reserve(vcount);
                        for (uint32_t v = 0; v < vcount; ++v) {
                            const uint8_t* vp = body + 9 + v * 12;
                            ent.vertices.push_back({ReadF32(vp), ReadF32(vp + 4), static_cast<double>(elev)});
                        }
                        ent.closed = (flagsLw & 0x01) != 0;
                        pushed = true;
                    }
                }
                break;
            }
            case kMETPolyline3D: {
                // Not emitted by the known writer, but the header/body-size
                // framing is self-describing regardless of type, so a 3-D
                // variant (x,y,z per vertex, no bulge) is a safe best-effort
                // read: "<BI>" flags,count + count*"<3f>" = 5 + count*12.
                if (bodySize >= 5) {
                    uint8_t flagsP = body[0];
                    uint32_t vcount = ReadU32(body + 1);
                    if (static_cast<size_t>(5) + static_cast<size_t>(vcount) * 12 <= bodySize) {
                        ent.type = SntEntity::Polyline;
                        ent.vertices.reserve(vcount);
                        for (uint32_t v = 0; v < vcount; ++v) {
                            const uint8_t* vp = body + 5 + v * 12;
                            ent.vertices.push_back({ReadF32(vp), ReadF32(vp + 4), ReadF32(vp + 8)});
                        }
                        ent.closed = (flagsP & 0x01) != 0;
                        pushed = true;
                    }
                }
                break;
            }
            case kMETArc:
                if (bodySize >= 24) {
                    ent.type = SntEntity::Arc;
                    ent.center = {ReadF32(body + 0), ReadF32(body + 4), ReadF32(body + 8)};
                    ent.radius = ReadF32(body + 12);
                    ent.startAngle = ReadF32(body + 16);
                    ent.endAngle = ReadF32(body + 20);
                    pushed = true;
                }
                break;
            case kMETCircle:
                if (bodySize >= 16) {
                    ent.type = SntEntity::Circle;
                    ent.center = {ReadF32(body + 0), ReadF32(body + 4), ReadF32(body + 8)};
                    ent.radius = ReadF32(body + 12);
                    pushed = true;
                }
                break;
            case kMETPoint:
                if (bodySize >= 12) {
                    ent.type = SntEntity::Point;
                    ent.vertices = {{ReadF32(body + 0), ReadF32(body + 4), ReadF32(body + 8)}};
                    pushed = true;
                }
                break;
            case kMETText:
            case kMETMText:
            case kMETDimension:
                if (bodySize >= 28) {
                    ent.type = SntEntity::Text;
                    double ix = ReadF32(body + 0), iy = ReadF32(body + 4), iz = ReadF32(body + 8);
                    ent.vertices = {{ix, iy, iz}};
                    ent.textHeight = ReadF32(body + 12);
                    // rotation at body+16 - SntEntity has no rotation field yet.
                    uint32_t strIdx = ReadU32(body + 20);
                    ent.text = (strIdx < strings.size()) ? strings[strIdx] : "";
                    pushed = true;
                }
                break;
            case kMETSolid:
            case kMETFace3d:
                if (bodySize >= 48) {
                    ent.type = SntEntity::ThreeDFace;
                    ent.vertices.resize(4);
                    for (int v = 0; v < 4; ++v) {
                        const uint8_t* vp = body + v * 12;
                        ent.vertices[v] = {ReadF32(vp), ReadF32(vp + 4), ReadF32(vp + 8)};
                    }
                    pushed = true;
                }
                break;
            case kMETEllipse:
            case kMETSpline:
            case kMETMesh:
            default:
                // Body layout not confirmed against a real file for these
                // (the known writer never emits them) - skip rather than
                // guess and risk rendering wrong geometry.
                break;
        }

        if (pushed) m_doc.entities.push_back(std::move(ent));
        ep = bodyEnd;
    }

    return true;
}

std::vector<SntEntity> SntAttachment::entitiesForLayer(const std::string& layerName) const {
    std::vector<SntEntity> result;
    for (const auto& ent : m_doc.entities) {
        if (ent.layer == layerName) result.push_back(ent);
    }
    return result;
}

std::vector<std::pair<std::string, int>> SntAttachment::layerStats() const {
    std::map<std::string, int> counts;
    for (const auto& ent : m_doc.entities) counts[ent.layer]++;
    std::vector<std::pair<std::string, int>> stats;
    for (const auto& [name, count] : counts) stats.push_back({name, count});
    return stats;
}

std::array<uint8_t, 3> SntAttachment::resolveEntityColor(int aci, int layerIdx) const {
    const auto& aciTable = AciColorTable::Instance();
    if (aci >= 1 && aci <= 255) {
        auto [r, g, b] = aciTable.Lookup(aci);
        return {r, g, b};
    }
    if (layerIdx >= 0 && layerIdx < static_cast<int>(m_doc.layers.size())) {
        return m_doc.layers[layerIdx].second;
    }
    return {255, 255, 255};
}

std::vector<SntEntity> SntAttachment::generateCircleVertices(const std::array<double, 3>& center, double radius, int segments) {
    std::vector<SntEntity> result;
    SntEntity ent;
    ent.type = SntEntity::Polyline;
    ent.vertices.resize(segments);
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * kPi * i / segments;
        ent.vertices[i] = {center[0] + radius * std::cos(angle), center[1] + radius * std::sin(angle), center[2]};
    }
    ent.closed = true;
    result.push_back(ent);
    return result;
}

std::vector<SntEntity> SntAttachment::generateArcVertices(const std::array<double, 3>& center, double radius, double startAngle, double endAngle, int segments) {
    std::vector<SntEntity> result;
    if (endAngle < startAngle) endAngle += 360.0;
    SntEntity ent;
    ent.type = SntEntity::Polyline;
    ent.vertices.resize(segments);
    double startRad = startAngle * kPi / 180.0;
    double endRad = endAngle * kPi / 180.0;
    for (int i = 0; i < segments; ++i) {
        double t = (i == segments - 1) ? 1.0 : static_cast<double>(i) / (segments - 1);
        double angle = startRad + t * (endRad - startRad);
        ent.vertices[i] = {center[0] + radius * std::cos(angle), center[1] + radius * std::sin(angle), center[2]};
    }
    result.push_back(ent);
    return result;
}

} // namespace cad
} // namespace workstation
