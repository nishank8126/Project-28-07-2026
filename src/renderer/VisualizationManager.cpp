#include "workstation/renderer/VisualizationManager.h"
#include "workstation/renderer/RenderContext.h"
#include "imgui.h"

namespace workstation {
namespace renderer {

void VisualizationManager::Initialize() {
    CreateDefaultPresets();
    InitializeClassificationPalette();

    modeNames_.resize(16);
    modeNames_[static_cast<int>(VisualizationMode::RGB)] = "RGB";
    modeNames_[static_cast<int>(VisualizationMode::Intensity)] = "Intensity";
    modeNames_[static_cast<int>(VisualizationMode::Classification)] = "Classification";
    modeNames_[static_cast<int>(VisualizationMode::Elevation)] = "Elevation";
    modeNames_[static_cast<int>(VisualizationMode::HeightRamp)] = "Height Ramp";
    modeNames_[static_cast<int>(VisualizationMode::Density)] = "Density";
    modeNames_[static_cast<int>(VisualizationMode::NormalShading)] = "Normal Shading";
    modeNames_[static_cast<int>(VisualizationMode::ReturnNumber)] = "Return Number";
    modeNames_[static_cast<int>(VisualizationMode::ScanAngle)] = "Scan Angle";
    modeNames_[static_cast<int>(VisualizationMode::gpsTime)] = "GPS Time";
    modeNames_[static_cast<int>(VisualizationMode::User)] = "User";
    modeNames_[static_cast<int>(VisualizationMode::Debug)] = "Debug";
    modeNames_[static_cast<int>(VisualizationMode::DepthShading)] = "Depth Shading";
    modeNames_[static_cast<int>(VisualizationMode::SurfaceShading)] = "Surface Shading";
    modeNames_[static_cast<int>(VisualizationMode::EyeDomeLighting)] = "Eye-Dome Lighting";
    modeNames_[static_cast<int>(VisualizationMode::ClassificationPalette)] = "Classification (Full)";
}

void VisualizationManager::SetMode(VisualizationMode mode) {
    currentMode_ = mode;
}

void VisualizationManager::SetIntensityRange(float minVal, float maxVal) {
    intensityMin_ = minVal;
    intensityMax_ = maxVal;
}

void VisualizationManager::SetElevationRange(float minVal, float maxVal) {
    elevationMin_ = minVal;
    elevationMax_ = maxVal;
}

void VisualizationManager::SetDepthShadingRange(float minDist, float maxDist) {
    depthMin_ = minDist;
    depthMax_ = maxDist;
}

void VisualizationManager::SetSurfaceShadingParams(float ambient, float diffuse, float specular, float shininess) {
    surfaceAmbient_ = ambient;
    surfaceDiffuse_ = diffuse;
    surfaceSpecular_ = specular;
    surfaceShininess_ = shininess;
}

void VisualizationManager::SetClassificationColor(int cls, float r, float g, float b) {
    if (cls >= 0 && cls < kClassificationClassCount) {
        classificationPalette_[cls] = {r, g, b};
    }
}

void VisualizationManager::InitializeClassificationPalette() {
    // ASPRS LAS 1.4 standard classification colours (RGB floats 0-1).
    // Reference: ASPRS LiDAR Specification 1.4 – R14, Table 9.
    classificationPalette_[0]  = {0.50f, 0.50f, 0.50f}; // 0  Created / Never classified
    classificationPalette_[1]  = {0.00f, 1.00f, 0.00f}; // 1  Unclassified
    classificationPalette_[2]  = {0.00f, 0.78f, 0.00f}; // 2  Ground
    classificationPalette_[3]  = {0.00f, 0.60f, 0.00f}; // 3  Low Vegetation
    classificationPalette_[4]  = {0.00f, 0.42f, 0.00f}; // 4  Medium Vegetation
    classificationPalette_[5]  = {0.00f, 0.25f, 0.00f}; // 5  High Vegetation
    classificationPalette_[6]  = {1.00f, 0.00f, 0.00f}; // 6  Building
    classificationPalette_[7]  = {1.00f, 0.50f, 0.00f}; // 7  Low Point (noise)
    classificationPalette_[8]  = {1.00f, 1.00f, 0.00f}; // 8  Reserved / Model Key-point
    classificationPalette_[9]  = {0.50f, 0.00f, 0.50f}; // 9  Water
    classificationPalette_[10] = {0.75f, 0.75f, 0.75f}; // 10 Reserved / Overlap Flight Path
    classificationPalette_[11] = {0.80f, 0.80f, 0.00f}; // 11 Reserved / Wire – Conductor
    classificationPalette_[12] = {0.60f, 0.60f, 0.00f}; // 12 Reserved / Wire – Fence
    classificationPalette_[13] = {0.40f, 0.40f, 0.00f}; // 13 Reserved / Wire – Transmission
    classificationPalette_[14] = {0.20f, 0.20f, 0.00f}; // 14 Reserved / Wire – Cable
    classificationPalette_[15] = {0.60f, 0.30f, 0.00f}; // 15 Reserved / Wire – Structure
    classificationPalette_[16] = {0.00f, 0.00f, 1.00f}; // 16 Reserved / Road Surface
    classificationPalette_[17] = {0.50f, 0.50f, 1.00f}; // 17 Reserved / Roof
}

const std::string& VisualizationManager::GetModeName(VisualizationMode mode) const {
    int idx = static_cast<int>(mode);
    if (idx >= 0 && idx < static_cast<int>(modeNames_.size())) {
        return modeNames_[idx];
    }
    static std::string unknown = "Unknown";
    return unknown;
}

void VisualizationManager::ApplyPreset(uint32_t index) {
    if (index < presets_.size()) {
        const auto& preset = presets_[index];
        currentMode_ = preset.mode;
        intensityMin_ = preset.intensityMin;
        intensityMax_ = preset.intensityMax;
        elevationMin_ = preset.elevationMin;
        elevationMax_ = preset.elevationMax;
    }
}

void VisualizationManager::CreateDefaultPresets() {
    presets_.clear();
    presets_.push_back({"Default RGB", VisualizationMode::RGB, 0.0f, 1.0f, 0.0f, 1000.0f, 0xFFFFFFFF, 0xFF0000FF});
    presets_.push_back({"High Contrast Intensity", VisualizationMode::Intensity, 0.0f, 1.0f, 0.0f, 1000.0f, 0xFF0000FF, 0xFFFF0000});
    presets_.push_back({"Terrain Classification", VisualizationMode::Classification, 0.0f, 1.0f, 0.0f, 1000.0f, 0xFF00FF00, 0xFFFF0000});
    presets_.push_back({"Elevation Rainbow", VisualizationMode::Elevation, 0.0f, 1.0f, 0.0f, 1000.0f, 0xFF0000FF, 0xFFFF0000});
    presets_.push_back({"Height Ramp", VisualizationMode::HeightRamp, 0.0f, 1.0f, 0.0f, 1000.0f, 0xFF0000FF, 0xFFFF0000});
}

void VisualizationManager::RenderUI(RenderContext& ctx) {
    ImGui::Text("Visualization");
    ImGui::Separator();

    int currentMode = static_cast<int>(currentMode_);
    if (ImGui::Combo("Mode", &currentMode, modeNames_[0].c_str(), static_cast<int>(modeNames_.size()))) {
        currentMode_ = static_cast<VisualizationMode>(currentMode);
    }

    if (currentMode_ == VisualizationMode::Intensity) {
        float minVal = intensityMin_;
        float maxVal = intensityMax_;
        if (ImGui::SliderFloat("Intensity Min", &minVal, 0.0f, 1.0f)) {
            intensityMin_ = minVal;
        }
        if (ImGui::SliderFloat("Intensity Max", &maxVal, 0.0f, 1.0f)) {
            intensityMax_ = maxVal;
        }
    }

    if (currentMode_ == VisualizationMode::Elevation || currentMode_ == VisualizationMode::HeightRamp) {
        float minVal = elevationMin_;
        float maxVal = elevationMax_;
        if (ImGui::SliderFloat("Elevation Min", &minVal, -1000.0f, 10000.0f)) {
            elevationMin_ = minVal;
        }
        if (ImGui::SliderFloat("Elevation Max", &maxVal, -1000.0f, 10000.0f)) {
            elevationMax_ = maxVal;
        }
    }

    if (currentMode_ == VisualizationMode::DepthShading || currentMode_ == VisualizationMode::EyeDomeLighting) {
        float minDist = depthMin_;
        float maxDist = depthMax_;
        if (ImGui::SliderFloat("Depth Min", &minDist, 0.0f, 10000.0f)) {
            depthMin_ = minDist;
        }
        if (ImGui::SliderFloat("Depth Max", &maxDist, 1.0f, 50000.0f)) {
            depthMax_ = maxDist;
        }
    }

    if (currentMode_ == VisualizationMode::SurfaceShading) {
        ImGui::SliderFloat("Ambient", &surfaceAmbient_, 0.0f, 1.0f);
        ImGui::SliderFloat("Diffuse", &surfaceDiffuse_, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &surfaceSpecular_, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &surfaceShininess_, 1.0f, 128.0f);
    }

    if (currentMode_ == VisualizationMode::EyeDomeLighting) {
        ImGui::SliderFloat("EDL Strength", &edlStrength_, 0.0f, 5.0f);
    }

    ImGui::Separator();
    ImGui::Text("Presets:");
    for (size_t i = 0; i < presets_.size(); ++i) {
        if (ImGui::Button(presets_[i].name.c_str())) {
            ApplyPreset(static_cast<uint32_t>(i));
        }
    }

    ImGui::Separator();
    bool darkBg = darkBackground_;
    if (ImGui::Checkbox("Dark Background", &darkBg)) {
        darkBackground_ = darkBg;
    }
}

} // namespace renderer
} // namespace workstation
