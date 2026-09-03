#pragma once
#include "workstation/scene/SceneManager.h"
#include <string>

namespace workstation {
namespace scene {

struct ProjectData {
    std::string projectName;
    std::string version = "1.0";

    struct ObjectEntry {
        std::string name;
        std::string filePath;
        std::string typeName;
        uint32_t layerID = 0;
        bool visible = true;
    };

    struct LayerEntry {
        std::string name;
        uint32_t id = 0;
        bool visible = true;
        float opacity = 1.0f;
        uint32_t color = 0xFFFFFFFF;
    };

    struct CameraEntry {
        double position[3] = {};
        double target[3] = {};
        double up[3] = {0, 0, 1};
        double fov = 45.0;
    };

    std::vector<ObjectEntry> objects;
    std::vector<LayerEntry> layers;
    CameraEntry camera;
};

class SceneSerializer {
public:
    static bool SaveProject(const std::string& filepath,
                             const SceneManager& scene,
                             std::string* error = nullptr);

    static bool LoadProject(const std::string& filepath,
                             ProjectData& data,
                             std::string* error = nullptr);

    static bool SaveProjectJson(const std::string& filepath,
                                 const SceneManager& scene,
                                 std::string* error = nullptr);

    static bool LoadProjectJson(const std::string& filepath,
                                 ProjectData& data,
                                 std::string* error = nullptr);
};

} // namespace scene
} // namespace workstation
