#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/AciColorTable.h"
#include <cmath>

namespace workstation {
namespace cad {

static constexpr double kPi = 3.14159265358979323846;

DxfAttachment::DxfAttachment(const std::string& filepath)
    : m_filePath(filepath), m_reader(std::make_unique<DxfFileReader>()) {}

std::string DxfAttachment::filename() const {
    auto pos = m_filePath.find_last_of("/\\");
    return (pos != std::string::npos) ? m_filePath.substr(pos + 1) : m_filePath;
}

bool DxfAttachment::load(std::string* errorMessage) {
    if (!m_reader->readFile(m_filePath, errorMessage)) {
        m_loaded = false;
        return false;
    }

    m_layerManager.clear();
    const auto& doc = m_reader->document();
    const auto& aci = AciColorTable::Instance();

    for (const auto& ent : doc.entities) {
        std::string layer = ent.layer.empty() ? "0" : ent.layer;
        uint8_t r, g, b;
        if (ent.hasTrueColor) {
            auto rgb = aci.TrueColorToRgb(ent.trueColor);
            r = rgb.r; g = rgb.g; b = rgb.b;
        } else {
            auto [rr, gg, bb] = aci.LookupNormalized(ent.color);
            r = static_cast<uint8_t>(rr * 255); g = static_cast<uint8_t>(gg * 255); b = static_cast<uint8_t>(bb * 255);
        }
        m_layerManager.addLayer(layer, r, g, b, 1);
    }

    m_loaded = true;
    return true;
}

DxfAttachmentGeometry DxfAttachment::buildGeometry() const {
    DxfAttachmentGeometry geom;
    if (!m_loaded || !m_reader) return geom;

    const auto& doc = m_reader->document();
    const auto& aci = AciColorTable::Instance();

    auto addSegment = [&](float x0, float y0, float z0, float x1, float y1, float z1, uint8_t r, uint8_t g, uint8_t b) {
        uint32_t base = static_cast<uint32_t>(geom.lineVertices.size() / 3);
        geom.lineVertices.insert(geom.lineVertices.end(), {x0, y0, z0 + static_cast<float>(m_zOffset)});
        geom.lineVertices.insert(geom.lineVertices.end(), {x1, y1, z1 + static_cast<float>(m_zOffset)});
        geom.lineIndices.insert(geom.lineIndices.end(), {base, base + 1});
        float fr = r / 255.0f, fg = g / 255.0f, fb = b / 255.0f;
        geom.lineColors.insert(geom.lineColors.end(), {fr, fg, fb});
        geom.lineColors.insert(geom.lineColors.end(), {fr, fg, fb});
    };

    for (const auto& ent : doc.entities) {
        if (!m_layerManager.isLayerVisible(ent.layer)) continue;

        uint8_t r, g, b;
        if (m_overrideEnabled) { r = m_overrideR; g = m_overrideG; b = m_overrideB; }
        else if (ent.hasTrueColor) { auto rgb = aci.TrueColorToRgb(ent.trueColor); r = rgb.r; g = rgb.g; b = rgb.b; }
        else { auto [rr, gg, bb] = aci.LookupNormalized(ent.color); r = rr * 255; g = gg * 255; b = bb * 255; }

        if (ent.type == "LINE" && ent.points.size() >= 2) {
            addSegment(ent.points[0].x, ent.points[0].y, ent.points[0].z,
                       ent.points[1].x, ent.points[1].y, ent.points[1].z, r, g, b);
        }
        else if (ent.type == "LWPOLYLINE" || ent.type == "POLYLINE") {
            int n = static_cast<int>(ent.points.size());
            if (n >= 2) {
                for (int i = 0; i < n - 1; ++i)
                    addSegment(ent.points[i].x, ent.points[i].y, ent.points[i].z,
                               ent.points[i+1].x, ent.points[i+1].y, ent.points[i+1].z, r, g, b);
                if (ent.closed && n > 2)
                    addSegment(ent.points[n-1].x, ent.points[n-1].y, ent.points[n-1].z,
                               ent.points[0].x, ent.points[0].y, ent.points[0].z, r, g, b);
            }
        }
        else if (ent.type == "CIRCLE") {
            auto pts = DxfFileReader::generateCirclePoints(ent.center, ent.radius, 64);
            for (size_t i = 0; i < pts.size(); ++i) {
                size_t next = (i + 1) % pts.size();
                addSegment(pts[i].x, pts[i].y, pts[i].z, pts[next].x, pts[next].y, pts[next].z, r, g, b);
            }
        }
        else if (ent.type == "ARC") {
            auto pts = DxfFileReader::generateArcPoints(ent.center, ent.radius, ent.startAngle, ent.endAngle);
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                addSegment(pts[i].x, pts[i].y, pts[i].z, pts[i+1].x, pts[i+1].y, pts[i+1].z, r, g, b);
            }
        }
        else if (ent.type == "3DFACE" && ent.points.size() >= 3) {
            for (size_t i = 0; i < ent.points.size(); ++i) {
                size_t next = (i + 1) % ent.points.size();
                addSegment(ent.points[i].x, ent.points[i].y, ent.points[i].z,
                           ent.points[next].x, ent.points[next].y, ent.points[next].z, r, g, b);
            }
        }
        else if ((ent.type == "TEXT" || ent.type == "MTEXT") && !ent.points.empty()) {
            const auto& pt = ent.points[0];
            geom.pointVertices.insert(geom.pointVertices.end(), {static_cast<float>(pt.x), static_cast<float>(pt.y), static_cast<float>(pt.z + m_zOffset)});
            geom.pointColors.insert(geom.pointColors.end(), {r / 255.0f, g / 255.0f, b / 255.0f});
        }
    }
    return geom;
}

void DxfAttachment::buildGeometryAsync(std::function<void(DxfAttachmentGeometry)> callback) {
    DxfAttachmentGeometry geom = buildGeometry();
    callback(geom);
}

std::vector<std::pair<std::string, int>> DxfAttachment::layerStats() const {
    std::vector<std::pair<std::string, int>> stats;
    if (!m_loaded || !m_reader) return stats;
    for (const auto& [name, count] : m_reader->document().layerEntityCounts) {
        stats.push_back({name, count});
    }
    return stats;
}

uint8_t DxfAttachment::resolveEntityColor(const DxfEntity& entity, uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (m_overrideEnabled) { r = m_overrideR; g = m_overrideG; b = m_overrideB; return 0; }
    const auto& aci = AciColorTable::Instance();
    if (entity.hasTrueColor) { auto rgb = aci.TrueColorToRgb(entity.trueColor); r = rgb.r; g = rgb.g; b = rgb.b; return 0; }
    auto [rr, gg, bb] = aci.LookupNormalized(entity.color);
    r = rr * 255; g = gg * 255; b = bb * 255;
    return entity.color;
}

} // namespace cad
} // namespace workstation
