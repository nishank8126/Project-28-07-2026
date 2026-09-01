#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <functional>

namespace workstation {
namespace renderer {

struct PipelineKey {
    VkShaderModule vertModule = VK_NULL_HANDLE;
    VkShaderModule fragModule = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    bool depthTest = true;
    bool depthWrite = true;
    bool blendEnabled = true;
    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    bool operator==(const PipelineKey& o) const {
        return vertModule == o.vertModule && fragModule == o.fragModule &&
               colorFormat == o.colorFormat && depthFormat == o.depthFormat &&
               depthTest == o.depthTest && depthWrite == o.depthWrite &&
               blendEnabled == o.blendEnabled && cullMode == o.cullMode &&
               topology == o.topology;
    }
};

struct PipelineKeyHash {
    size_t operator()(const PipelineKey& k) const {
        size_t h = 0;
        h ^= std::hash<VkShaderModule>()(k.vertModule) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<VkShaderModule>()(k.fragModule) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(k.colorFormat) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(k.depthFormat) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(k.depthTest) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(k.depthWrite) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<bool>()(k.blendEnabled) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(k.cullMode) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(k.topology) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

class PipelineCacheManager {
public:
    ~PipelineCacheManager();

    bool Initialize(VkDevice device, VkRenderPass renderPass);
    void Shutdown();

    VkPipeline GetOrCreateGraphicsPipeline(
        const PipelineKey& key,
        VkPipelineLayout layout,
        const std::vector<VkPipelineShaderStageCreateInfo>& stages);

    VkPipeline GetOrCreateComputePipeline(
        VkShaderModule computeModule,
        VkPipelineLayout layout);

    void DestroyAll();
    uint32_t GetCacheSize() const { return static_cast<uint32_t>(cache_.size()); }

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> cache_;
    std::vector<VkPipeline> computePipelines_;
};

} // namespace renderer
} // namespace workstation
