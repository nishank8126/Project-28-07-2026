#include "workstation/scene/CadSceneObject.h"
#include "workstation/cad/DxfAttachment.h"
#include "workstation/cad/DwgAttachment.h"
#include "workstation/cad/SntAttachment.h"
#include "workstation/renderer/CadRenderer.h"
#include <cstdio>
#include <algorithm>

namespace workstation {
namespace scene {

CadSceneObject::~CadSceneObject() { Unload(); }

bool CadSceneObject::Load(std::string* error) {
    loaded_ = true;
    return true;
}

void CadSceneObject::Unload() {
    loaded_ = false;
}

void CadSceneObject::Update() {
    if (!loaded_) return;
    auto bounds = GetBounds();
    if (node_) node_->SetBounds(bounds);
}

spatial::BoundingBox CadSceneObject::GetBounds() const {
    spatial::BoundingBox b = {};
    if (cadRenderer_) {
        const auto& atts = cadRenderer_->GetAttachments();
        for (const auto& att : atts) {
            if ((dxfAtt_ && att.dxf == dxfAtt_) ||
                (dwgAtt_ && att.dwg == dwgAtt_) ||
                (sntAtt_ && att.snt == sntAtt_)) {
                b.minX = std::min(b.minX, att.bounds.minX);
                b.minY = std::min(b.minY, att.bounds.minY);
                b.minZ = std::min(b.minZ, att.bounds.minZ);
                b.maxX = std::max(b.maxX, att.bounds.maxX);
                b.maxY = std::max(b.maxY, att.bounds.maxY);
                b.maxZ = std::max(b.maxZ, att.bounds.maxZ);
            }
        }
    }
    return b;
}

void CadSceneObject::SubmitRenderCommands(renderer::RenderQueue& queue,
                                            void* linePipeline,
                                            void* pipelineLayout) {
    (void)queue;
    (void)linePipeline;
    (void)pipelineLayout;
}

cad::LayerManager* CadSceneObject::GetLayerManager() {
    if (dxfAtt_) return dxfAtt_->layerManager();
    if (dwgAtt_) return dwgAtt_->layerManager();
    if (sntAtt_) return sntAtt_->layerManager();
    return nullptr;
}

void CadSceneObject::SetLayerVisibility(const std::string& layer, bool visible) {
    auto* lm = GetLayerManager();
    if (lm) lm->setLayerVisible(layer, visible);
}

bool CadSceneObject::IsLayerVisible(const std::string& layer) const {
    const cad::LayerManager* lm = nullptr;
    if (dxfAtt_) lm = dxfAtt_->layerManager();
    else if (dwgAtt_) lm = dwgAtt_->layerManager();
    else if (sntAtt_) lm = sntAtt_->layerManager();
    return lm ? lm->isLayerVisible(layer) : true;
}

void CadSceneObject::SetOverrideColor(uint8_t r, uint8_t g, uint8_t b) {
    if (dxfAtt_) dxfAtt_->setOverrideColor(r, g, b);
}

void CadSceneObject::ClearOverrideColor() {
    if (dxfAtt_) dxfAtt_->clearOverrideColor();
}

void CadSceneObject::SetZOffset(double offset) {
    if (dxfAtt_) dxfAtt_->setZOffset(offset);
    if (sntAtt_) sntAtt_->setZOffset(offset);
}

double CadSceneObject::GetZOffset() const {
    if (dxfAtt_) return dxfAtt_->zOffset();
    if (sntAtt_) return sntAtt_->zOffset();
    return 0.0;
}

} // namespace scene
} // namespace workstation
