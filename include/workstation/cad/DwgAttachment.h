#pragma once
#include "workstation/cad/DxfFileReader.h"
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/LayerManager.h"

#include <string>
#include <vector>
#include <memory>

namespace workstation {
namespace cad {

struct PrjBlock {
    std::string name;
    std::vector<std::pair<double, double>> polygon;
    double centroidX = 0, centroidY = 0;
};

class DwgAttachment {
public:
    explicit DwgAttachment(const std::string& filepath);
    ~DwgAttachment() = default;

    bool load(std::string* errorMessage = nullptr);
    bool isLoaded() const { return m_loaded; }

    const std::string& filepath() const { return m_filePath; }
    std::string filename() const;
    LayerManager* layerManager() { return &m_layerManager; }

    void setZOffset(double offset) { m_zOffset = offset; }
    double zOffset() const { return m_zOffset; }

    DxfAttachmentGeometry buildGeometry() const;

    static std::vector<PrjBlock> parsePrjBlocks(const std::string& prjPath);
    static std::string findAdjacentPrj(const std::string& dwgPath);

private:
    bool loadViaOdaConverter(std::string* errorMessage);
    bool loadViaDirectRead(std::string* errorMessage);

    std::string m_filePath;
    bool m_loaded = false;
    double m_zOffset = 0.1;

    std::unique_ptr<DxfFileReader> m_reader;
    LayerManager m_layerManager;
    std::vector<PrjBlock> m_prjBlocks;
};

} // namespace cad
} // namespace workstation
