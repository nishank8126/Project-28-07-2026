#pragma once
#include "workstation/vulkan/VulkanHeaders.h"
#include "workstation/vulkan/VulkanAllocator.h"
#include "workstation/gpu/GPUBufferManager.h"

#include <cstdint>

namespace workstation {
namespace renderer {

struct CameraUniforms {
    float viewProjection[16];
    float view[16];
    float projection[16];
    float cameraPosition[4];
    float cameraDirection[4];
    float lightDirection[4];
    float padding[4];
};

struct PointRenderUniforms {
    float pointScale;
    float pointSize;
    uint32_t visualizationMode;
    float intensityMin;
    float intensityMax;
    float elevationMin;
    float elevationMax;
    uint32_t padding0;
    uint32_t padding1;
    float colorRamp[8][4];
};

struct ConstantBufferManager {
    gpu::GPUBufferAllocation* cameraBuffer = nullptr;
    gpu::GPUBufferAllocation* renderParamsBuffer = nullptr;

    bool Initialize(gpu::GPUBufferManager& bufferManager);
    void Shutdown();

    void UpdateCamera(const CameraUniforms& data);
    void UpdateRenderParams(const PointRenderUniforms& data);

    void BindCamera(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                     uint32_t set, uint32_t binding);
    void BindRenderParams(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                           uint32_t set, uint32_t binding);

    VkDescriptorSet AllocateAndBind(VkDescriptorPool pool,
                                     VkDescriptorSetLayout layout,
                                     uint32_t frameIndex);
};

} // namespace renderer
} // namespace workstation
