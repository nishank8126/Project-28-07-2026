#pragma once
#include "workstation/vulkan/VulkanHeaders.h"

namespace workstation {
namespace vulkan {

struct ShaderSource {
    std::vector<uint32_t> spirv;
    VkShaderStageFlagBits stage;
    std::string entryPoint = "main";
};

class VulkanShaderManager {
public:
    ~VulkanShaderManager();

    bool Initialize(VkDevice device);
    void Shutdown();

    VkShaderModule CreateShaderModule(const uint32_t* code, size_t size);
    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& spirv);
    void DestroyShaderModule(VkShaderModule module);

    ShaderModule LoadSPIRV(const std::string& filepath, VkShaderStageFlagBits stage);
    std::vector<ShaderModule> LoadSPIRVFiles(
        const std::vector<std::pair<std::string, VkShaderStageFlagBits>>& files);

    static std::vector<uint32_t> ReadSPIRVFile(const std::string& filepath);

private:
    VkDevice device_ = VK_NULL_HANDLE;
};

} // namespace vulkan
} // namespace workstation
