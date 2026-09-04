#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace workstation {
namespace renderer {

class RenderContext;

// NOTE: this order must match shaders/point.vert and point.frag's
// `switch (visualizationMode)` case numbers exactly.
enum class VisualizationMode {
    RGB,                // 0
    Intensity,          // 1
    Classification,     // 2
    Elevation,          // 3
    HeightRamp,         // 4
    NormalShading,      // 5
    Density,            // 6
    ReturnNumber,       // 7
    ScanAngle,          // 8
    gpsTime,            // 9
    User,               // 10
    Debug,              // 11
    DepthShading,       // 12 - distance-from-camera colouring
    SurfaceShading,     // 13 - normals + depth composite
    EyeDomeLighting,    // 14 - screen-space edge-darkening (simplified)
    ClassificationPalette, // 15 - full ASPRS palette (18 classes)
    SurfaceDebug,          // 16 - green wireframe surface debug
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

// ASPRS LAS 1.4 classification palette (18 standard classes).
// Each entry is an {R,G,B} float triple.
struct ClassificationPaletteEntry {
    float r, g, b;
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

    // Depth / Surface shading parameters
    void SetDepthShadingRange(float minDist, float maxDist);
    float GetDepthShadingMin() const { return depthMin_; }
    float GetDepthShadingMax() const { return depthMax_; }

    void SetSurfaceShadingParams(float ambient, float diffuse, float specular, float shininess);
    float GetSurfaceAmbient() const { return surfaceAmbient_; }
    float GetSurfaceDiffuse() const { return surfaceDiffuse_; }
    float GetSurfaceSpecular() const { return surfaceSpecular_; }
    float GetSurfaceShininess() const { return surfaceShininess_; }

    // Eye-dome lighting strength
    void SetEDLStrength(float s) { edlStrength_ = s; }
    float GetEDLStrength() const { return edlStrength_; }

    // ASPRS classification palette (18 classes)
    static constexpr int kClassificationClassCount = 18;
    const ClassificationPaletteEntry* GetClassificationPalette() const { return classificationPalette_; }
    void SetClassificationColor(int cls, float r, float g, float b);

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

    // Depth shading
    float depthMin_ = 0.0f;
    float depthMax_ = 1000.0f;
    // Surface shading
    float surfaceAmbient_ = 0.2f;
    float surfaceDiffuse_ = 0.7f;
    float surfaceSpecular_ = 0.3f;
    float surfaceShininess_ = 32.0f;
    // Eye-dome lighting
    float edlStrength_ = 1.0f;

    // ASPRS LAS 1.4 classification palette (18 standard classes)
    ClassificationPaletteEntry classificationPalette_[kClassificationClassCount] = {};

    std::vector<VisualizationPreset> presets_;
    std::vector<std::string> modeNames_;

    void CreateDefaultPresets();
    void InitializeClassificationPalette();
};

} // namespace renderer
} // namespace workstation
