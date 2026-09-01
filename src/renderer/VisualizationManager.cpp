#include "workstation/renderer/VisualizationManager.h"
#include "workstation/renderer/RenderContext.h"
#include "imgui.h"

namespace workstation {
namespace renderer {

void VisualizationManager::Initialize() {
    CreateDefaultPresets();

    modeNames_.resize(11);
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
