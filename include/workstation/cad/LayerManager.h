#pragma once
#include <vector>
#include <string>
#include <map>
#include <set>
#include <utility>
#include <cstdint>

namespace workstation {
namespace cad {

struct Rgb { uint8_t r, g, b; };

struct LayerInfo {
    std::string name;
    uint8_t colorR = 255, colorG = 255, colorB = 255;
    bool visible = true;
    float lineWidth = 2.0f;
    std::string lineStyle = "Solid";
    int entityCount = 0;

    bool isSystemLayer() const;
};

class LayerManager {
public:
    LayerManager() = default;
    ~LayerManager() = default;

    void addLayer(const std::string& name, uint8_t r, uint8_t g, uint8_t b, int entityCount = 0);
    void removeLayer(const std::string& name);
    void setLayerVisible(const std::string& name, bool visible);
    void setLayerColor(const std::string& name, uint8_t r, uint8_t g, uint8_t b);
    void setLayerLineWidth(const std::string& name, float width);
    void setLayerLineStyle(const std::string& name, const std::string& style);

    void selectAll();
    void selectNone();
    void invertSelection();

    bool isLayerVisible(const std::string& name) const;
    std::tuple<uint8_t, uint8_t, uint8_t> layerColor(const std::string& name) const;
    float layerLineWidth(const std::string& name) const;
    std::string layerLineStyle(const std::string& name) const;
    int layerEntityCount(const std::string& name) const;

    std::vector<LayerInfo> layers() const;
    std::set<std::string> visibleLayers() const;
    std::set<std::string> allLayerNames() const;
    int layerCount() const;
    int visibleLayerCount() const;
    bool hasSystemLayers() const;

    void setAllLayersVisible(bool visible);
    void clear();

private:
    std::map<std::string, LayerInfo> m_layers;
    std::vector<std::string> m_order;
};

} // namespace cad
} // namespace workstation
