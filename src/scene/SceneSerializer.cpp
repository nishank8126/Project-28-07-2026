#include "workstation/scene/SceneSerializer.h"
#include <fstream>
#include <sstream>
#include <cstdio>

namespace workstation {
namespace scene {

bool SceneSerializer::SaveProject(const std::string& filepath,
                                    const SceneManager& scene,
                                    std::string* error) {
    return SaveProjectJson(filepath, scene, error);
}

bool SceneSerializer::LoadProject(const std::string& filepath,
                                    ProjectData& data,
                                    std::string* error) {
    return LoadProjectJson(filepath, data, error);
}

bool SceneSerializer::SaveProjectJson(const std::string& filepath,
                                        const SceneManager& scene,
                                        std::string* error) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        if (error) *error = "Cannot create file: " + filepath;
        return false;
    }

    file << "{\n";
    file << "  \"version\": \"1.0\",\n";
    file << "  \"projectName\": \"NakshaProject\",\n";

    file << "  \"layers\": [\n";
    const auto& layers = scene.GetLayers();
    bool firstLayer = true;
    for (const auto& [id, layer] : layers) {
        if (!firstLayer) file << ",\n";
        firstLayer = false;
        file << "    {\"id\": " << id
             << ", \"name\": \"" << layer.name
             << "\", \"visible\": " << (layer.visible ? "true" : "false")
             << ", \"opacity\": " << layer.opacity
             << ", \"color\": " << layer.color << "}";
    }
    file << "\n  ],\n";

    file << "  \"objects\": [\n";
    bool firstObj = true;
    scene.ForEachObject([&](const SceneObject* obj) {
        if (!obj) return;
        if (!firstObj) file << ",\n";
        firstObj = false;
        file << "    {\"name\": \"" << obj->GetDisplayName()
             << "\", \"path\": \"" << obj->GetFilePath()
             << "\", \"type\": " << static_cast<int>(obj->GetType())
             << ", \"nodeID\": " << obj->GetNodeID() << "}";
    });
    file << "\n  ]\n";
    file << "}\n";

    return true;
}

bool SceneSerializer::LoadProjectJson(const std::string& filepath,
                                        ProjectData& data,
                                        std::string* error) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        if (error) *error = "Cannot open file: " + filepath;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    data.projectName.clear();
    data.objects.clear();
    data.layers.clear();

    auto namePos = content.find("\"projectName\"");
    if (namePos != std::string::npos) {
        auto colonPos = content.find(':', namePos);
        auto quoteStart = content.find('"', colonPos + 1);
        auto quoteEnd = content.find('"', quoteStart + 1);
        if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            data.projectName = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    size_t objStart = content.find("\"objects\"");
    if (objStart != std::string::npos) {
        size_t bracketStart = content.find('[', objStart);
        size_t bracketEnd = content.find(']', bracketStart);
        if (bracketStart != std::string::npos && bracketEnd != std::string::npos) {
            std::string objectsStr = content.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
            size_t pos = 0;
            while ((pos = objectsStr.find('{', pos)) != std::string::npos) {
                size_t end = objectsStr.find('}', pos);
                if (end == std::string::npos) break;
                std::string objStr = objectsStr.substr(pos, end - pos + 1);
                pos = end + 1;

                ProjectData::ObjectEntry entry;

                auto nameP = objStr.find("\"name\"");
                if (nameP != std::string::npos) {
                    auto cs = objStr.find(':', nameP);
                    auto qs = objStr.find('"', cs + 1);
                    auto qe = objStr.find('"', qs + 1);
                    if (qs != std::string::npos && qe != std::string::npos) {
                        entry.name = objStr.substr(qs + 1, qe - qs - 1);
                    }
                }

                auto pathP = objStr.find("\"path\"");
                if (pathP != std::string::npos) {
                    auto cs = objStr.find(':', pathP);
                    auto qs = objStr.find('"', cs + 1);
                    auto qe = objStr.find('"', qs + 1);
                    if (qs != std::string::npos && qe != std::string::npos) {
                        entry.filePath = objStr.substr(qs + 1, qe - qs - 1);
                    }
                }

                data.objects.push_back(entry);
            }
        }
    }

    return true;
}

} // namespace scene
} // namespace workstation
