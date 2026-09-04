#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <fstream>
#include <sstream>

namespace workstation {
namespace display {

struct ClassEntry {
    int code = 0;
    std::string description;
    std::string drawMode;
    std::string level;
    std::array<uint8_t, 3> color = {128, 128, 128};
    float weight = 1.0f;
    bool visible = true;

    float normalizedR() const { return color[0] / 255.0f; }
    float normalizedG() const { return color[1] / 255.0f; }
    float normalizedB() const { return color[2] / 255.0f; }
};

using ClassPalette = std::unordered_map<int, ClassEntry>;

struct ViewSlot {
    ClassPalette palette;
    std::unordered_map<int, bool> visibility;
    float borderPercent = 0.0f;
    int borderMode = 1;
};

class PtcFileReader {
public:
    static bool load(const std::string& filepath, ClassPalette& outPalette, std::string* error = nullptr) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            if (error) *error = "Cannot open file: " + filepath;
            return false;
        }

        outPalette.clear();
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }

        for (size_t i = 0; i + 1 < lines.size(); i += 2) {
            std::istringstream header(lines[i]);
            std::string token;
            std::vector<std::string> headerFields;
            while (std::getline(header, token, '\t')) headerFields.push_back(token);
            if (headerFields.size() < 2) continue;

            std::istringstream detail(lines[i + 1]);
            std::vector<std::string> detailFields;
            while (std::getline(detail, token, '\t')) detailFields.push_back(token);
            if (detailFields.size() < 5) continue;

            ClassEntry entry;
            entry.code = std::stoi(headerFields[0]);
            // ENEL PTC format: description is in field[2] (field[1] is empty).
            // Standard PTC: description in field[1].
            entry.description = headerFields.size() > 1 && !headerFields[1].empty()
                ? headerFields[1]
                : (headerFields.size() > 2 ? headerFields[2] : "");
            entry.level = headerFields.size() > 3 ? headerFields[3] : "";
            entry.drawMode = detailFields.size() > 1 ? detailFields[1] : "";

            if (detailFields.size() > 3) {
                std::istringstream rgbStream(detailFields[3]);
                std::string rgbToken;
                int idx = 0;
                while (std::getline(rgbStream, rgbToken, ',') && idx < 3) {
                    entry.color[idx++] = static_cast<uint8_t>(std::stoi(rgbToken));
                }
            }

            // PTC visibility: some files use "0"/"1" booleans, others use
            // weight/priority values (0,1,2,5,7). Treat "0" and empty as
            // hidden; everything else as visible.
            entry.visible = true;
            if (detailFields.size() > 4) {
                const auto& vis = detailFields[4];
                entry.visible = !vis.empty() && vis != "0";
            }
            entry.weight = detailFields.size() > 5 ? std::stof(detailFields[5]) : 1.0f;

            outPalette[entry.code] = entry;
        }
        return true;
    }

    static bool save(const std::string& filepath, const ClassPalette& palette, std::string* error = nullptr) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            if (error) *error = "Cannot create file: " + filepath;
            return false;
        }

        for (const auto& [code, entry] : palette) {
            file << code << "\t" << entry.description << "\t" << entry.level << "\n";
            file << "*\t" << entry.drawMode << "\t" << code << "\t"
                 << static_cast<int>(entry.color[0]) << ","
                 << static_cast<int>(entry.color[1]) << ","
                 << static_cast<int>(entry.color[2]) << "\t"
                 << (entry.visible ? "1" : "0") << "\t"
                 << entry.weight << "\n\n";
        }
        return true;
    }
};

class DisplayModeManager {
public:
    enum class Mode {
        Classification = 0,
        ShadedClassification = 1,
        Depth = 2,
        Intensity = 3,
        RGB = 4,
        Elevation = 5,
        Surface = 6,
        Line = 7
    };

    enum class BorderMode {
        PerPoint = 0,
        Structured = 1,
        Hybrid = 2
    };

    static constexpr int SLOT_COUNT = 6;
    static constexpr int MAIN_SLOT = 0;

    DisplayModeManager() {
        for (int i = 0; i < SLOT_COUNT; ++i) {
            slots_[i] = ViewSlot{};
        }
    }

    Mode currentMode() const { return currentMode_; }
    void setCurrentMode(Mode mode) { currentMode_ = mode; }

    int currentSlot() const { return currentSlot_; }
    void setCurrentSlot(int slot) { if (slot >= 0 && slot < SLOT_COUNT) currentSlot_ = slot; }

    ViewSlot& slot(int idx) { return slots_[idx]; }
    const ViewSlot& slot(int idx) const { return slots_[idx]; }

    ClassPalette& mainPalette() { return slots_[MAIN_SLOT].palette; }
    const ClassPalette& mainPalette() const { return slots_[MAIN_SLOT].palette; }

    void setBorderPercent(int slot, float percent) {
        if (slot >= 0 && slot < SLOT_COUNT) slots_[slot].borderPercent = percent;
    }
    float borderPercent(int slot) const {
        return (slot >= 0 && slot < SLOT_COUNT) ? slots_[slot].borderPercent : 0.0f;
    }

    void setBorderMode(int slot, BorderMode mode) {
        if (slot >= 0 && slot < SLOT_COUNT) slots_[slot].borderMode = static_cast<int>(mode);
    }
    BorderMode borderMode(int slot) const {
        return static_cast<BorderMode>((slot >= 0 && slot < SLOT_COUNT) ? slots_[slot].borderMode : 1);
    }

    void setClassVisible(int slot, int classCode, bool visible) {
        if (slot >= 0 && slot < SLOT_COUNT) {
            slots_[slot].palette[classCode].visible = visible;
            slots_[slot].visibility[classCode] = visible;
        }
    }

    void setClassWeight(int slot, int classCode, float weight) {
        if (slot >= 0 && slot < SLOT_COUNT) {
            auto& pal = slots_[slot].palette;
            auto it = pal.find(classCode);
            if (it != pal.end()) it->second.weight = weight;
        }
    }

    void setClassColor(int slot, int classCode, uint8_t r, uint8_t g, uint8_t b) {
        if (slot >= 0 && slot < SLOT_COUNT) {
            auto& pal = slots_[slot].palette;
            auto it = pal.find(classCode);
            if (it != pal.end()) {
                it->second.color = {r, g, b};
            }
        }
    }

    void syncFromMainPalette() {
        for (int i = 1; i < SLOT_COUNT; ++i) {
            for (auto& [code, entry] : slots_[i].palette) {
                auto it = slots_[MAIN_SLOT].palette.find(code);
                if (it != slots_[MAIN_SLOT].palette.end()) {
                    entry.weight = it->second.weight;
                }
            }
        }
    }

    void loadPtc(const std::string& filepath, std::string* error = nullptr) {
        ClassPalette palette;
        if (PtcFileReader::load(filepath, palette, error)) {
            slots_[MAIN_SLOT].palette = palette;
            for (auto& [code, entry] : slots_[MAIN_SLOT].palette) {
                slots_[MAIN_SLOT].visibility[code] = entry.visible;
            }
            ptcPath_ = filepath;
        }
    }

    void savePtc(const std::string& filepath, std::string* error = nullptr) {
        PtcFileReader::save(filepath, slots_[MAIN_SLOT].palette, error);
        ptcPath_ = filepath;
    }

    const std::string& ptcPath() const { return ptcPath_; }

    int visibleClassCount(int slot) const {
        int count = 0;
        if (slot >= 0 && slot < SLOT_COUNT) {
            for (const auto& [code, entry] : slots_[slot].palette) {
                if (entry.visible) ++count;
            }
        }
        return count;
    }

    std::vector<int> visibleClassCodes(int slot) const {
        std::vector<int> codes;
        if (slot >= 0 && slot < SLOT_COUNT) {
            for (const auto& [code, entry] : slots_[slot].palette) {
                if (entry.visible) codes.push_back(code);
            }
        }
        return codes;
    }

    static const char* modeName(Mode mode) {
        switch (mode) {
            case Mode::Classification:          return "By Classification";
            case Mode::ShadedClassification:    return "Shaded Classification";
            case Mode::Depth:                   return "Depth";
            case Mode::Intensity:               return "Intensity";
            case Mode::RGB:                     return "RGB";
            case Mode::Elevation:               return "Elevation";
            case Mode::Surface:                 return "Surface";
            case Mode::Line:                    return "Line";
        }
        return "Unknown";
    }

    static const char* borderModeName(BorderMode mode) {
        switch (mode) {
            case BorderMode::PerPoint:    return "Per-Point";
            case BorderMode::Structured:  return "Structured";
            case BorderMode::Hybrid:      return "Hybrid";
        }
        return "Unknown";
    }

private:
    Mode currentMode_ = Mode::Classification;
    int currentSlot_ = 0;
    ViewSlot slots_[SLOT_COUNT];
    std::string ptcPath_;
};

} // namespace display
} // namespace workstation
