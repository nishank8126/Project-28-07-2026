#pragma once
#include "workstation/cad/LayerManager.h"
#include <array>
#include <tuple>

namespace workstation {
namespace cad {

class AciColorTable {
public:
    static const AciColorTable& Instance();

    Rgb Lookup(int aci) const;
    std::tuple<float, float, float> LookupNormalized(int aci) const;

    Rgb TrueColorToRgb(uint32_t tc) const;

    static Rgb ByLayer() { return {255, 255, 255}; }
    static Rgb ByBlock() { return {255, 255, 255}; }
    static constexpr int ByLayerCode = 256;
    static constexpr int ByBlockCode = 0;

private:
    AciColorTable();
    std::array<Rgb, 256> m_table;
};

} // namespace cad
} // namespace workstation
