#pragma once
#include "workstation/scene/SceneNode.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace workstation {
namespace renderer { class RenderQueue; }

namespace scene {

enum class ObjectType : uint32_t {
    Unknown = 0,
    PointCloud = 1,
    CadAttachment = 2,
    ShadingDisplay = 3,
    Mesh = 4,
    Annotation = 5,
    Measurement = 6,
    Section = 7,
    Raster = 8
};

class SceneObject {
public:
    SceneObject() = default;
    virtual ~SceneObject() = default;

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;

    virtual ObjectType GetType() const = 0;
    virtual const char* GetTypeName() const = 0;

    virtual bool Load(std::string* error = nullptr) = 0;
    virtual void Unload() = 0;
    virtual void Update() = 0;
    virtual bool IsLoaded() const = 0;

    virtual spatial::BoundingBox GetBounds() const = 0;

    virtual void SubmitRenderCommands(renderer::RenderQueue& queue,
                                       void* linePipeline,
                                       void* pipelineLayout) = 0;

    SceneNode* GetNode() const { return node_; }
    void SetNode(SceneNode* node) { node_ = node; }

    NodeID GetNodeID() const { return node_ ? node_->GetID() : 0; }

    uint64_t GetRevision() const { return revision_; }
    void IncrementRevision() { ++revision_; }

    void SetFilePath(const std::string& path) { filePath_ = path; }
    const std::string& GetFilePath() const { return filePath_; }

    void SetDisplayName(const std::string& name) { displayName_ = name; }
    const std::string& GetDisplayName() const { return displayName_; }

    bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

protected:
    SceneNode* node_ = nullptr;
    std::string filePath_;
    std::string displayName_;
    uint64_t revision_ = 0;
    bool dirty_ = true;
};

} // namespace scene
} // namespace workstation
