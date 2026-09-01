#include "workstation/renderer/ConstantBufferManager.h"

namespace workstation {
namespace renderer {

bool ConstantBufferManager::Initialize(gpu::GPUBufferManager& bufferManager) {
    cameraBuffer = bufferManager.Allocate(gpu::BufferType::Uniform,
                                            sizeof(CameraUniforms), true);
    renderParamsBuffer = bufferManager.Allocate(gpu::BufferType::Uniform,
                                                  sizeof(PointRenderUniforms), true);
    return cameraBuffer && renderParamsBuffer;
}

void ConstantBufferManager::Shutdown() {
    cameraBuffer = nullptr;
    renderParamsBuffer = nullptr;
}

void ConstantBufferManager::UpdateCamera(const CameraUniforms& data) {
    if (cameraBuffer && cameraBuffer->info.pMappedData) {
        memcpy(cameraBuffer->info.pMappedData, &data, sizeof(CameraUniforms));
    }
}

void ConstantBufferManager::UpdateRenderParams(const PointRenderUniforms& data) {
    if (renderParamsBuffer && renderParamsBuffer->info.pMappedData) {
        memcpy(renderParamsBuffer->info.pMappedData, &data, sizeof(PointRenderUniforms));
    }
}

void ConstantBufferManager::BindCamera(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                                         uint32_t set, uint32_t binding) {
    (void)cmd; (void)bindPoint; (void)set; (void)binding;
}

void ConstantBufferManager::BindRenderParams(VkCommandBuffer cmd,
                                               VkPipelineBindPoint bindPoint,
                                               uint32_t set, uint32_t binding) {
    (void)cmd; (void)bindPoint; (void)set; (void)binding;
}

VkDescriptorSet ConstantBufferManager::AllocateAndBind(VkDescriptorPool pool,
                                                         VkDescriptorSetLayout layout,
                                                         uint32_t frameIndex) {
    (void)pool; (void)layout; (void)frameIndex;
    return VK_NULL_HANDLE;
}

} // namespace renderer
} // namespace workstation
