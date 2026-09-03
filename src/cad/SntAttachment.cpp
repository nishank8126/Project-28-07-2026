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

bool SntAttachment::loadModern(const uint8_t* data, size_t size, std::string* errorMessage) {
    if (size < 0x60) {
        if (errorMessage) *errorMessage = "SNT file too small for modern header";
        return false;
    }

    if (std::memcmp(data, "SNT ", 4) != 0 && std::memcmp(data, "SNT\x01", 4) != 0) {
        if (errorMessage) *errorMessage = "Not a valid SNT file (bad magic)";
        return false;
    }

    uint32_t entityCount = 0;
    std::memcpy(&entityCount, data + 0x08, 4);

    double bboxMinX, bboxMinY, bboxMaxX, bboxMaxY;
    std::memcpy(&bboxMinX, data + 0x10, 8);
    std::memcpy(&bboxMinY, data + 0x18, 8);
    std::memcpy(&bboxMaxX, data + 0x20, 8);
    std::memcpy(&bboxMaxY, data + 0x28, 8);

    m_doc.bboxMinX = bboxMinX; m_doc.bboxMinY = bboxMinY;
    m_doc.bboxMaxX = bboxMaxX; m_doc.bboxMaxY = bboxMaxY;
    m_doc.hasBBox = true;

    uint32_t entityTableOffset = 0, entityTableEnd = 0;
    std::memcpy(&entityTableOffset, data + 0x50, 4);
    std::memcpy(&entityTableEnd, data + 0x58, 4);

    if (entityTableOffset == 0 || entityTableEnd <= entityTableOffset || entityTableEnd > size) {
        if (errorMessage) *errorMessage = "Invalid entity table offsets";
        return false;
    }

    double marginX = (bboxMaxX - bboxMinX) * 0.05 + 1.0;
    double marginY = (bboxMaxY - bboxMinY) * 0.05 + 1.0;
    double loX = bboxMinX - marginX, hiX = bboxMaxX + marginX;
    double loY = bboxMinY - marginY, hiY = bboxMaxY + marginY;

    size_t pos = entityTableOffset;
    int recoveredCount = 0;
    while (pos + 4 <= entityTableEnd) {
        uint32_t count = 0;
        std::memcpy(&count, data + pos, 4);
        size_t vertexBytes = static_cast<size_t>(count) * 12;

        if (count >= 2 && count <= 100000 && pos + 4 + vertexBytes <= entityTableEnd) {
            bool plausible = true;
            for (uint32_t v = 0; v < count && plausible; ++v) {
                float x, y;
                std::memcpy(&x, data + pos + 4 + v * 12 + 0, 4);
                std::memcpy(&y, data + pos + 4 + v * 12 + 4, 4);
                if (x < loX || x > hiX || y < loY || y > hiY) plausible = false;
            }

            if (plausible) {
                SntEntity ent;
                ent.type = SntEntity::Polyline;
                ent.colorR = 255; ent.colorG = 255; ent.colorB = 255;
                ent.layer = "0";
                ent.vertices.resize(count);
                for (uint32_t v = 0; v < count; ++v) {
                    float x, y, z;
                    std::memcpy(&x, data + pos + 4 + v * 12 + 0, 4);
                    std::memcpy(&y, data + pos + 4 + v * 12 + 4, 4);
                    std::memcpy(&z, data + pos + 4 + v * 12 + 8, 4);
                    ent.vertices[v] = {static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)};
                }
                m_doc.entities.push_back(ent);
                recoveredCount++;
                pos += 4 + vertexBytes;
                continue;
            }
        }
        pos += 4;
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
