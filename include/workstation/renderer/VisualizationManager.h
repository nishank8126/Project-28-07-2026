#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace workstation {
namespace renderer {

class RenderContext;

// NOTE: this order must match shaders/point.vert and point.frag's
// `switch (visualizationMode)` case numbers exactly (their case bodies were
// authored in this order: RGB=0 .. NormalShading=5, Density=6), not the
// order the modes happen to be listed/described elsewhere.
enum class VisualizationMode {
    RGB,
    Intensity,
    Classification,
    Elevation,
    HeightRamp,
    NormalShading,
    Density,
    ReturnNumber,
    ScanAngle,
    gpsTime,
    User,
    Debug
};

struct VisualizationPreset {
    std::string name;
    VisualizationMode mode;
    float intensityMin = 0.0f;
    float intensityMax = 1.0f;
    float elevationMin = 0.0f;
    float elevationMax = 1000.0f;
    uint32_t primaryColor = 0xFFFFFFFF;
    uint32_t secondaryColor = 0xFF0000FF;
};

class VisualizationManager {
public:
    VisualizationManager() = default;
    ~VisualizationManager() = default;

    void Initialize();

    void SetMode(VisualizationMode mode);
    VisualizationMode GetMode() const { return currentMode_; }

    void SetIntensityRange(float minVal, float maxVal);
    void SetElevationRange(float minVal, float maxVal);
    float GetIntensityMin() const { return intensityMin_; }
    float GetIntensityMax() const { return intensityMax_; }
    float GetElevationMin() const { return elevationMin_; }
    float GetElevationMax() const { return elevationMax_; }

    void SetCustomColor(uint32_t color) { customColor_ = color; }
    uint32_t GetCustomColor() const { return customColor_; }

    void SetBackgroundMode(bool dark) { darkBackground_ = dark; }
    bool IsDarkBackground() const { return darkBackground_; }

    const std::string& GetModeName(VisualizationMode mode) const;
    const std::vector<VisualizationPreset>& GetPresets() const { return presets_; }
    void ApplyPreset(uint32_t index);

    void RenderUI(RenderContext& ctx);

private:
    VisualizationMode currentMode_ = VisualizationMode::RGB;
    float intensityMin_ = 0.0f;
    float intensityMax_ = 1.0f;
    float elevationMin_ = 0.0f;
    float elevationMax_ = 1000.0f;
    uint32_t customColor_ = 0xFFFFFFFF;
    bool darkBackground_ = true;

    std::vector<VisualizationPreset> presets_;
    std::vector<std::string> modeNames_;

    void CreateDefaultPresets();
};

} // namespace renderer
} // namespace workstation
