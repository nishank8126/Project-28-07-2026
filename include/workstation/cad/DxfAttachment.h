#pragma once
#include "workstation/cad/DxfFileReader.h"
#include "workstation/cad/LayerManager.h"
#include "workstation/cad/AciColorTable.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

namespace workstation {
namespace cad {

struct DxfAttachmentGeometry {
    std::vector<float> lineVertices;
    std::vector<uint32_t> lineIndices;
    std::vector<float> lineColors;
    std::vector<float> pointVertices;
    std::vector<float> pointColors;
    std::vector<float> textPositions;
    std::vector<std::string> textStrings;
};

class DxfAttachment {
public:
    enum class DisplayMode { Overlay, Underlay };

    explicit DxfAttachment(const std::string& filepath);
    ~DxfAttachment() = default;

    bool load(std::string* errorMessage = nullptr);
    bool isLoaded() const { return m_loaded; }

    const std::string& filepath() const { return m_filePath; }
    std::string filename() const;
    const DxfDocument& document() const { return m_reader ? m_reader->document() : m_emptyDoc; }

    LayerManager* layerManager() { return &m_layerManager; }

    void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    DisplayMode displayMode() const { return m_displayMode; }

    void setOverrideColor(uint8_t r, uint8_t g, uint8_t b) { m_overrideR = r; m_overrideG = g; m_overrideB = b; m_overrideEnabled = true; }
    void clearOverrideColor() { m_overrideEnabled = false; }
    bool isOverrideEnabled() const { return m_overrideEnabled; }

    void setZOffset(double offset) { m_zOffset = offset; }
    double zOffset() const { return m_zOffset; }

    DxfAttachmentGeometry buildGeometry() const;
    void buildGeometryAsync(std::function<void(DxfAttachmentGeometry)> callback);

    std::vector<std::pair<std::string, int>> layerStats() const;
    uint8_t resolveEntityColor(const DxfEntity& entity, uint8_t& r, uint8_t& g, uint8_t& b) const;

private:
    std::string m_filePath;
    bool m_loaded = false;
    DisplayMode m_displayMode = DisplayMode::Overlay;
    uint8_t m_overrideR = 255, m_overrideG = 0, m_overrideB = 0;
    bool m_overrideEnabled = false;
    double m_zOffset = 0.1;

    std::unique_ptr<DxfFileReader> m_reader;
    LayerManager m_layerManager;
    DxfDocument m_emptyDoc;
};

} // namespace cad
} // namespace workstation
