#include "workstation/cad/DxfFileReader.h"
#include <fstream>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <set>

namespace workstation {
namespace cad {

static constexpr double kPi = 3.14159265358979323846;
static constexpr int kDefaultCircleSegments = 64;

bool DxfFileReader::readFile(const std::string& filepath, std::string* errorMessage) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file) {
        if (errorMessage) *errorMessage = "Cannot open file: " + filepath;
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0);
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        if (errorMessage) *errorMessage = "Failed to read file: " + filepath;
        return false;
    }
    file.close();
    return readFromBuffer(buffer, errorMessage);
}

bool DxfFileReader::readFromBuffer(const std::vector<char>& buffer, std::string* errorMessage) {
    resetParser();
    const char* data = buffer.data();
    size_t size = buffer.size();
    size_t pos = 0;

    while (pos < size) {
        size_t lineStart = pos;
        while (pos < size && data[pos] != '\n' && data[pos] != '\r') ++pos;
        size_t lineLen = pos - lineStart;
        while (lineLen > 0 && (data[lineStart + lineLen - 1] == ' ' || data[lineStart + lineLen - 1] == '\t' || data[lineStart + lineLen - 1] == '\r')) --lineLen;
        std::string line(data + lineStart, lineLen);
        if (pos < size) ++pos;
        if (line.empty()) continue;
        if (line == "EOF") break;

        if (line == "SECTION") {
            m_state.expectStringValue = true;
            continue;
        }
        if (m_state.expectStringValue) {
            m_state.expectStringValue = false;
            std::string sectionName = line;
            if (sectionName.size() >= 2 && sectionName.front() == '"' && sectionName.back() == '"')
                sectionName = sectionName.substr(1, sectionName.size() - 2);
            if (sectionName == "ENTITIES") m_state.inEntities = true;
            else if (sectionName == "TABLES") m_state.inTables = true;
            else if (sectionName == "HEADER") m_state.inHeader = true;
            else if (sectionName == "BLOCKS") m_state.inBlocks = true;
            continue;
        }
        if (line == "ENDSEC") {
            if (m_state.inEntities && m_state.hasPendingEntity) {
                m_doc.entities.push_back(m_state.pendingEntity);
                m_state.hasPendingEntity = false;
                m_state.inPolyline = false;
            }
            m_state.inEntities = m_state.inTables = m_state.inHeader = m_state.inBlocks = false;
            continue;
        }
        if (line == "POLYLINE") {
            if (m_state.inEntities && m_state.hasPendingEntity) {
                m_doc.entities.push_back(m_state.pendingEntity);
                m_state.hasPendingEntity = false;
            }
            m_state.inPolyline = true;
            m_state.polylinePoints.clear();
            m_state.polylineClosed = false;
            m_state.pendingEntity = DxfEntity{};
            m_state.pendingEntity.type = "POLYLINE";
            m_state.pendingEntity.layer = m_state.currentLayer;
            m_state.hasPendingEntity = true;
            continue;
        }
        if (line == "SEQEND") {
            if (m_state.inEntities && m_state.inPolyline && m_state.hasPendingEntity) {
                auto& ent = m_state.pendingEntity;
                ent.points = m_state.polylinePoints;
                ent.closed = m_state.polylineClosed;
                int n = static_cast<int>(ent.points.size());
                for (int i = 0; i < n - 1; ++i) ent.segments.push_back({i, i + 1});
                if (ent.closed && n > 2) ent.segments.push_back({n - 1, 0});
                m_doc.entities.push_back(ent);
                m_state.hasPendingEntity = false;
            }
            m_state.inPolyline = false;
            m_state.polylinePoints.clear();
            continue;
        }
        if (m_state.inPolyline && line == "VERTEX") {
            if (m_state.hasPendingEntity) {
                m_doc.entities.push_back(m_state.pendingEntity);
            }
            m_state.pendingEntity = DxfEntity{};
            m_state.pendingEntity.type = "VERTEX";
            m_state.pendingEntity.layer = m_state.currentLayer;
            m_state.hasPendingEntity = true;
            continue;
        }
        if (!m_state.inEntities) continue;

        int code = 0;
        auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(), code);
        if (ec != std::errc{}) continue;

        size_t nextLineStart = pos;
        while (nextLineStart < size) {
            size_t s = nextLineStart;
            while (nextLineStart < size && data[nextLineStart] != '\n' && data[nextLineStart] != '\r') ++nextLineStart;
            if (nextLineStart < size) ++nextLineStart;
            size_t l = nextLineStart - s;
            while (l > 0 && (data[s + l - 1] == ' ' || data[s + l - 1] == '\t' || data[s + l - 1] == '\r')) --l;
            std::string valLine(data + s, l);
            if (!valLine.empty()) {
                pos = nextLineStart;
                parseEntity(code, valLine);
                break;
            }
        }
    }

    computeBoundingBox();
    if (m_doc.entities.empty() && errorMessage) *errorMessage = "No entities found in DXF file";
    return !m_doc.entities.empty();
}

void DxfFileReader::resetParser() {
    m_doc = DxfDocument{};
    m_state = ParserState{};
}

bool DxfFileReader::parseEntity(int code, const std::string& value) {
    if (code == 0) {
        static const std::set<std::string> validTypes = {
            "LINE", "LWPOLYLINE", "CIRCLE", "ARC", "SPLINE", "POINT",
            "3DFACE", "TEXT", "MTEXT", "INSERT", "ELLIPSE", "SOLID"
        };
        if (validTypes.count(value)) {
            if (m_state.hasPendingEntity && !m_state.inPolyline) {
                m_doc.entities.push_back(m_state.pendingEntity);
                m_state.hasPendingEntity = false;
            }
            m_state.pendingEntity = DxfEntity{};
            m_state.pendingEntity.type = value;
            m_state.pendingEntity.layer = m_state.currentLayer;
            m_state.pendingEntity.color = m_state.currentColor;
            m_state.hasPendingEntity = true;
            m_state.currentLayer = "0";
            m_state.currentColor = 7;
        }
        return true;
    }
    if (!m_state.hasPendingEntity && !m_state.inPolyline) return true;

    auto& ent = m_state.pendingEntity;

    switch (code) {
        case 8:
            ent.layer = value;
            if (!m_state.inPolyline) m_state.currentLayer = value;
            break;
        case 62: {
            int c = 0;
            std::from_chars(value.data(), value.data() + value.size(), c);
            ent.color = std::abs(c);
            if (!m_state.inPolyline) m_state.currentColor = ent.color;
            break;
        }
        case 420: {
            uint32_t tc = 0;
            std::from_chars(value.data(), value.data() + value.size(), tc);
            ent.trueColor = tc;
            ent.hasTrueColor = true;
            break;
        }
        case 10: case 11: case 12: case 13: {
            double v = 0;
            std::from_chars(value.data(), value.data() + value.size(), v);
            if (m_state.inPolyline) {
                m_state.polylinePoints.push_back({v, 0, 0});
            } else {
                int idx = (code - 10) / 10;
                while (static_cast<int>(ent.points.size()) <= idx) ent.points.push_back({0, 0, 0});
                ent.points[idx].x = v;
            }
            break;
        }
        case 20: case 21: case 22: case 23: {
            double v = 0;
            std::from_chars(value.data(), value.data() + value.size(), v);
            if (m_state.inPolyline) {
                if (!m_state.polylinePoints.empty()) m_state.polylinePoints.back().y = v;
            } else {
                int idx = (code - 20) / 10;
                if (static_cast<int>(ent.points.size()) > idx) ent.points[idx].y = v;
            }
            break;
        }
        case 30: case 31: case 32: case 33: {
            double v = 0;
            std::from_chars(value.data(), value.data() + value.size(), v);
            if (m_state.inPolyline) {
                if (!m_state.polylinePoints.empty()) m_state.polylinePoints.back().z = v;
            } else {
                int idx = (code - 30) / 10;
                if (static_cast<int>(ent.points.size()) > idx) ent.points[idx].z = v;
            }
            break;
        }
        case 40: { double v = 0; std::from_chars(value.data(), value.data() + value.size(), v); ent.radius = v; break; }
        case 50: { double v = 0; std::from_chars(value.data(), value.data() + value.size(), v); ent.startAngle = v; break; }
        case 51: { double v = 0; std::from_chars(value.data(), value.data() + value.size(), v); ent.endAngle = v; break; }
        case 70: {
            int v = 0;
            std::from_chars(value.data(), value.data() + value.size(), v);
            if (m_state.inPolyline) m_state.polylineClosed = (v & 1) != 0;
            ent.closed = (v & 1) != 0;
            break;
        }
        case 1: ent.text = value; break;
        case 44: { double v = 0; std::from_chars(value.data(), value.data() + value.size(), v); ent.textHeight = v; break; }
        default: break;
    }
    return true;
}

void DxfFileReader::computeBoundingBox() {
    if (m_doc.entities.empty()) return;
    double minX = 1e30, minY = 1e30, minZ = 1e30;
    double maxX = -1e30, maxY = -1e30, maxZ = -1e30;
    bool any = false;
    for (const auto& ent : m_doc.entities) {
        for (const auto& pt : ent.points) {
            minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y); maxY = std::max(maxY, pt.y);
            minZ = std::min(minZ, pt.z); maxZ = std::max(maxZ, pt.z);
            any = true;
        }
        if (ent.type == "CIRCLE" || ent.type == "ARC") {
            minX = std::min(minX, ent.center.x - ent.radius); maxX = std::max(maxX, ent.center.x + ent.radius);
            minY = std::min(minY, ent.center.y - ent.radius); maxY = std::max(maxY, ent.center.y + ent.radius);
            any = true;
        }
    }
    if (any) {
        m_doc.bboxMinX = minX; m_doc.bboxMinY = minY; m_doc.bboxMinZ = minZ;
        m_doc.bboxMaxX = maxX; m_doc.bboxMaxY = maxY; m_doc.bboxMaxZ = maxZ;
        m_doc.hasBBox = true;
    }
}

std::vector<std::string> DxfFileReader::layerNames() const {
    std::vector<std::string> names;
    for (const auto& [name, count] : m_doc.layerEntityCounts) names.push_back(name);
    return names;
}

int DxfFileReader::layerEntityCount(const std::string& layer) const {
    auto it = m_doc.layerEntityCounts.find(layer);
    return it != m_doc.layerEntityCounts.end() ? it->second : 0;
}

std::vector<DxfPoint3d> DxfFileReader::generateCirclePoints(const DxfPoint3d& center, double radius, int segments) {
    std::vector<DxfPoint3d> pts(segments);
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * kPi * i / segments;
        pts[i] = {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle), center.z};
    }
    return pts;
}

std::vector<DxfPoint3d> DxfFileReader::generateArcPoints(const DxfPoint3d& center, double radius, double startAngle, double endAngle, int segments) {
    if (endAngle < startAngle) endAngle += 360.0;
    if (segments <= 0) segments = std::max(32, static_cast<int>((endAngle - startAngle) / 5.0));
    std::vector<DxfPoint3d> pts(segments);
    double startRad = startAngle * kPi / 180.0;
    double endRad = endAngle * kPi / 180.0;
    for (int i = 0; i < segments; ++i) {
        double t = (i == segments - 1) ? 1.0 : static_cast<double>(i) / (segments - 1);
        double angle = startRad + t * (endRad - startRad);
        pts[i] = {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle), center.z};
    }
    return pts;
}

void DxfFileReader::forEachEntity(EntityCallback callback) const {
    for (const auto& ent : m_doc.entities) callback(ent);
}

} // namespace cad
} // namespace workstation
