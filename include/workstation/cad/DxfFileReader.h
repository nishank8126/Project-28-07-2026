#pragma once
#include "workstation/cad/LayerManager.h"
#include "workstation/cad/AciColorTable.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace workstation {
namespace cad {

struct DxfPoint3d {
    double x = 0, y = 0, z = 0;
};

struct DxfEntity {
    std::string type;
    std::string layer;
    int color = 7;
    std::vector<DxfPoint3d> points;
    std::vector<std::pair<int, int>> segments;
    double radius = 0;
    double startAngle = 0;
    double endAngle = 0;
    DxfPoint3d center;
    std::string text;
    double textHeight = 2.5;
    bool closed = false;
    double lineWidth = 2.0;
    std::string lineStyle = "Solid";
    uint32_t trueColor = 0;
    bool hasTrueColor = false;
};

struct DxfDocument {
    std::vector<DxfEntity> entities;
    std::map<std::string, std::tuple<uint8_t, uint8_t, uint8_t>> layerColors;
    std::map<std::string, int> layerEntityCounts;
    double bboxMinX = 0, bboxMinY = 0, bboxMinZ = 0;
    double bboxMaxX = 0, bboxMaxY = 0, bboxMaxZ = 0;
    bool hasBBox = false;
};

class DxfFileReader {
public:
    DxfFileReader() = default;
    ~DxfFileReader() = default;

    bool readFile(const std::string& filepath, std::string* errorMessage = nullptr);
    bool readFromBuffer(const std::vector<char>& buffer, std::string* errorMessage = nullptr);

    const DxfDocument& document() const { return m_doc; }
    DxfDocument& document() { return m_doc; }

    int entityCount() const { return static_cast<int>(m_doc.entities.size()); }
    std::vector<std::string> layerNames() const;
    int layerEntityCount(const std::string& layer) const;

    static std::vector<DxfPoint3d> generateCirclePoints(const DxfPoint3d& center, double radius, int segments = 64);
    static std::vector<DxfPoint3d> generateArcPoints(const DxfPoint3d& center, double radius, double startAngle, double endAngle, int segments = 0);

    using EntityCallback = std::function<void(const DxfEntity&)>;
    void forEachEntity(EntityCallback callback) const;

    void computeBoundingBox();

private:
    bool parseEntity(int code, const std::string& value);
    void resetParser();

    DxfDocument m_doc;

    struct ParserState {
        bool inEntities = false;
        bool inTables = false;
        bool inHeader = false;
        bool inBlocks = false;
        DxfEntity pendingEntity;
        bool hasPendingEntity = false;
        std::string currentLayer;
        int currentColor = 7;
        bool inPolyline = false;
        std::vector<DxfPoint3d> polylinePoints;
        bool polylineClosed = false;
        bool expectStringValue = false;
    } m_state;
};

} // namespace cad
} // namespace workstation
