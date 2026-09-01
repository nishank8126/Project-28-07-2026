#pragma once
#include "workstation/renderer/RenderCommand.h"
#include "workstation/renderer/PipelineCacheManager.h"

#include <vector>
#include <cstdint>
#include <algorithm>

namespace workstation {
namespace renderer {

struct RenderBatch {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    std::vector<RenderCommand> commands;
    uint32_t totalPoints = 0;

    bool IsCompatible(const RenderCommand& cmd) const {
        return pipeline == cmd.pipeline &&
               pipelineLayout == cmd.pipelineLayout &&
               descriptorSet == cmd.descriptorSet;
    }
};

struct RenderQueueStats {
    uint32_t totalCommands = 0;
    uint32_t totalBatches = 0;
    uint32_t totalDrawCalls = 0;
    uint64_t totalPoints = 0;
    uint32_t culledCommands = 0;
    uint32_t pipelineChanges = 0;
    uint32_t descriptorChanges = 0;
};

class RenderQueue {
public:
    void BeginFrame();
    void SubmitCommand(RenderCommand& cmd);
    void EndFrame();

    void SortByPipeline();
    void SortByDistance();
    void SortByPipelineThenDistance();

    const std::vector<RenderBatch>& GetBatches() const { return batches_; }
    const RenderQueueStats& GetStats() const { return stats_; }

    void SetSortMode(bool sortByPipeline) { sortByPipeline_ = sortByPipeline; }
    bool HasWork() const { return !batches_.empty(); }

    void Clear();
    void Reserve(uint32_t estimatedCommands);

private:
    std::vector<RenderCommand> pendingCommands_;
    std::vector<RenderBatch> batches_;
    RenderQueueStats stats_ = {};
    bool sortByPipeline_ = true;

    void BuildBatches();
};

} // namespace renderer
} // namespace workstation
