#pragma once
#include "workstation/cad/LayerManager.h"
#include "workstation/cad/AciColorTable.h"

#include <string>
#include <vector>
#include <map>
#include <array>
#include <cstdint>

namespace workstation {
namespace cad {

struct SntEntity {
    enum Type { Polyline = 0, Text = 1, ThreeDFace = 2, Arc = 3, Circle = 4, Point = 5, Unsupported = 99 };

    Type type = Unsupported;
    std::string layer;
    uint8_t colorR = 255, colorG = 255, colorB = 255;
    std::vector<std::array<double, 3>> vertices;
    bool closed = false;
    std::string text;
    double textHeight = 2.5;
    double radius = 0;
    double startAngle = 0;
    double endAngle = 0;
    std::array<double, 3> center = {0, 0, 0};
    int lineweight = 0;
};

struct SntDocument {
    int versionMajor = 0;
    int versionMinor = 0;
    std::vector<std::string> strings;
    std::vector<SntEntity> entities;
    std::vector<std::pair<std::string, std::array<uint8_t, 3>>> layers;
    double bboxMinX = 0, bboxMinY = 0, bboxMaxX = 0, bboxMaxY = 0;
    bool hasBBox = false;
};

class SntAttachment {
public:
    explicit SntAttachment(const std::string& filepath);
    ~SntAttachment() = default;

    bool load(std::string* errorMessage = nullptr);
    bool isLoaded() const { return m_loaded; }

    const std::string& filepath() const { return m_filePath; }
    std::string filename() const;
    const SntDocument& document() const { return m_doc; }

    LayerManager* layerManager() { return &m_layerManager; }

    void setZOffset(double offset) { m_zOffset = offset; }
    double zOffset() const { return m_zOffset; }

    std::vector<SntEntity> entitiesForLayer(const std::string& layerName) const;
    int entityCount() const { return static_cast<int>(m_doc.entities.size()); }
    int layerCount() const { return static_cast<int>(m_doc.layers.size()); }

    std::vector<std::pair<std::string, int>> layerStats() const;
    std::array<uint8_t, 3> resolveEntityColor(int aci, int layerIdx) const;

    static std::vector<SntEntity> generateCircleVertices(const std::array<double, 3>& center, double radius, int segments = 36);
    static std::vector<SntEntity> generateArcVertices(const std::array<double, 3>& center, double radius, double startAngle, double endAngle, int segments = 32);

private:
    bool loadLegacyV0(const uint8_t* data, size_t size, std::string* errorMessage);
    bool loadModern(const uint8_t* data, size_t size, std::string* errorMessage);

    std::string m_filePath;
    bool m_loaded = false;
    double m_zOffset = 0.1;
    SntDocument m_doc;
    LayerManager m_layerManager;
};

} // namespace cad
} // namespace workstation
