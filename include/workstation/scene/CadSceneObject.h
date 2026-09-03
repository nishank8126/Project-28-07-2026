#pragma once
#include "workstation/scene/SceneObject.h"
#include "workstation/cad/LayerManager.h"

#include <memory>
#include <string>
#include <vector>

namespace workstation {
namespace renderer { class RenderQueue; class CadRenderer; }
namespace cad { class DxfAttachment; class DwgAttachment; class SntAttachment; }

namespace scene {

class CadSceneObject : public SceneObject {
public:
    enum class CadType { DXF, DWG, SNT };

    CadSceneObject() = default;
    ~CadSceneObject() override;

    ObjectType GetType() const override { return ObjectType::CadAttachment; }
    const char* GetTypeName() const override { return "CAD Attachment"; }

    bool Load(std::string* error = nullptr) override;
    void Unload() override;
    void Update() override;
    bool IsLoaded() const override { return loaded_; }

    spatial::BoundingBox GetBounds() const override;

    void SubmitRenderCommands(renderer::RenderQueue& queue,
                               void* linePipeline,
                               void* pipelineLayout) override;

    void SetCadType(CadType type) { cadType_ = type; }
    CadType GetCadType() const { return cadType_; }

    void SetDxfAttachment(cad::DxfAttachment* att) { dxfAtt_ = att; }
    void SetDwgAttachment(cad::DwgAttachment* att) { dwgAtt_ = att; }
    void SetSntAttachment(cad::SntAttachment* att) { sntAtt_ = att; }

    cad::DxfAttachment* GetDxfAttachment() const { return dxfAtt_; }
    cad::DwgAttachment* GetDwgAttachment() const { return dwgAtt_; }
    cad::SntAttachment* GetSntAttachment() const { return sntAtt_; }

    cad::LayerManager* GetLayerManager();
    void SetLayerVisibility(const std::string& layer, bool visible);
    bool IsLayerVisible(const std::string& layer) const;

    void SetCadRenderer(renderer::CadRenderer* renderer) { cadRenderer_ = renderer; }
    renderer::CadRenderer* GetCadRenderer() const { return cadRenderer_; }

    void SetOverrideColor(uint8_t r, uint8_t g, uint8_t b);
    void ClearOverrideColor();

    void SetZOffset(double offset);
    double GetZOffset() const;

private:
    CadType cadType_ = CadType::DXF;
    cad::DxfAttachment* dxfAtt_ = nullptr;
    cad::DwgAttachment* dwgAtt_ = nullptr;
    cad::SntAttachment* sntAtt_ = nullptr;
    renderer::CadRenderer* cadRenderer_ = nullptr;
    bool loaded_ = false;
};

} // namespace scene
} // namespace workstation
