#include "workstation/cad/LayerManager.h"
#include <algorithm>
#include <cctype>

namespace workstation {
namespace cad {

static std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

bool LayerInfo::isSystemLayer() const {
    static const std::set<std::string> tokens = {
        "BL", "FILENAMES", "BLOCKS", "FEATUREATTRIBS",
        "BLOCK_LABEL", "BLOCKLABEL", "FILENAME"
    };
    std::string upper = toUpper(name);
    if (tokens.count(upper)) return true;
    for (const auto& tok : tokens) {
        if (upper.find(tok) != std::string::npos) return true;
    }
    return false;
}

void LayerManager::addLayer(const std::string& name, uint8_t r, uint8_t g, uint8_t b, int entityCount) {
    if (name.empty()) return;
    auto it = m_layers.find(name);
    if (it == m_layers.end()) {
        m_order.push_back(name);
        m_layers[name] = LayerInfo{name, r, g, b, true, 2.0f, "Solid", entityCount};
    } else {
        it->second.entityCount += entityCount;
        if (it->second.colorR == 255 && it->second.colorG == 255 && it->second.colorB == 255 && (r != 255 || g != 255 || b != 255)) {
            it->second.colorR = r; it->second.colorG = g; it->second.colorB = b;
        }
    }
}

void LayerManager::removeLayer(const std::string& name) {
    if (m_layers.erase(name) > 0) {
        m_order.erase(std::remove(m_order.begin(), m_order.end(), name), m_order.end());
    }
}

void LayerManager::setLayerVisible(const std::string& name, bool visible) {
    auto it = m_layers.find(name);
    if (it != m_layers.end()) it->second.visible = visible;
}

void LayerManager::setLayerColor(const std::string& name, uint8_t r, uint8_t g, uint8_t b) {
    auto it = m_layers.find(name);
    if (it != m_layers.end()) {
        it->second.colorR = r; it->second.colorG = g; it->second.colorB = b;
    }
}

void LayerManager::setLayerLineWidth(const std::string& name, float width) {
    auto it = m_layers.find(name);
    if (it != m_layers.end()) it->second.lineWidth = std::max(1.0f, std::min(width, 20.0f));
}

void LayerManager::setLayerLineStyle(const std::string& name, const std::string& style) {
    auto it = m_layers.find(name);
    if (it != m_layers.end()) {
        static const std::set<std::string> valid = {"Solid", "Dashed", "Dotted", "Dash-Dot"};
        it->second.lineStyle = valid.count(style) ? style : "Solid";
    }
}

void LayerManager::selectAll() {
    for (auto& [name, info] : m_layers) info.visible = true;
}

void LayerManager::selectNone() {
    for (auto& [name, info] : m_layers) {
        if (!info.isSystemLayer()) info.visible = false;
    }
}

void LayerManager::invertSelection() {
    for (auto& [name, info] : m_layers) {
        if (!info.isSystemLayer()) info.visible = !info.visible;
    }
}

bool LayerManager::isLayerVisible(const std::string& name) const {
    auto it = m_layers.find(name);
    return it != m_layers.end() ? it->second.visible : true;
}

std::tuple<uint8_t, uint8_t, uint8_t> LayerManager::layerColor(const std::string& name) const {
    auto it = m_layers.find(name);
    if (it != m_layers.end()) return {it->second.colorR, it->second.colorG, it->second.colorB};
    return {255, 255, 255};
}

float LayerManager::layerLineWidth(const std::string& name) const {
    auto it = m_layers.find(name);
    return it != m_layers.end() ? it->second.lineWidth : 2.0f;
}

std::string LayerManager::layerLineStyle(const std::string& name) const {
    auto it = m_layers.find(name);
    return it != m_layers.end() ? it->second.lineStyle : "Solid";
}

int LayerManager::layerEntityCount(const std::string& name) const {
    auto it = m_layers.find(name);
    return it != m_layers.end() ? it->second.entityCount : 0;
}

std::vector<LayerInfo> LayerManager::layers() const {
    std::vector<LayerInfo> result;
    result.reserve(m_order.size());
    for (const auto& name : m_order) {
        auto it = m_layers.find(name);
        if (it != m_layers.end()) result.push_back(it->second);
    }
    return result;
}

std::set<std::string> LayerManager::visibleLayers() const {
    std::set<std::string> result;
    for (const auto& [name, info] : m_layers) {
        if (info.visible) result.insert(name);
    }
    return result;
}

std::set<std::string> LayerManager::allLayerNames() const {
    std::set<std::string> result;
    for (const auto& [name, info] : m_layers) result.insert(name);
    return result;
}

int LayerManager::layerCount() const { return static_cast<int>(m_layers.size()); }

int LayerManager::visibleLayerCount() const {
    int count = 0;
    for (const auto& [name, info] : m_layers) {
        if (info.visible) ++count;
    }
    return count;
}

bool LayerManager::hasSystemLayers() const {
    for (const auto& [name, info] : m_layers) {
        if (info.isSystemLayer()) return true;
    }
    return false;
}

void LayerManager::setAllLayersVisible(bool visible) {
    for (auto& [name, info] : m_layers) info.visible = visible;
}

void LayerManager::clear() {
    m_layers.clear();
    m_order.clear();
}

} // namespace cad
} // namespace workstation
