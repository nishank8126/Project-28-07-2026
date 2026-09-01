#include "workstation/renderer/RenderQueue.h"
#include <algorithm>

namespace workstation {
namespace renderer {

void RenderQueue::BeginFrame() {
    pendingCommands_.clear();
    batches_.clear();
    stats_ = {};
}

void RenderQueue::SubmitCommand(RenderCommand& cmd) {
    if (!cmd.geometry || cmd.pointCount == 0) return;
    pendingCommands_.push_back(cmd);
    stats_.totalCommands++;
    stats_.totalPoints += cmd.pointCount;
}

void RenderQueue::EndFrame() {
    BuildBatches();
}

void RenderQueue::BuildBatches() {
    if (pendingCommands_.empty()) return;

    if (sortByPipeline_) {
        std::sort(pendingCommands_.begin(), pendingCommands_.end(),
                  [](const RenderCommand& a, const RenderCommand& b) {
                      return a.GetSortKey() < b.GetSortKey();
                  });
    }

    batches_.clear();
    RenderBatch currentBatch{};

    for (auto& cmd : pendingCommands_) {
        if (currentBatch.commands.empty() || currentBatch.IsCompatible(cmd)) {
            currentBatch.pipeline = cmd.pipeline;
            currentBatch.pipelineLayout = cmd.pipelineLayout;
            currentBatch.descriptorSet = cmd.descriptorSet;
            currentBatch.commands.push_back(cmd);
            currentBatch.totalPoints += cmd.pointCount;
        } else {
            batches_.push_back(std::move(currentBatch));
            currentBatch = RenderBatch{};
            currentBatch.pipeline = cmd.pipeline;
            currentBatch.pipelineLayout = cmd.pipelineLayout;
            currentBatch.descriptorSet = cmd.descriptorSet;
            currentBatch.commands.push_back(cmd);
            currentBatch.totalPoints = cmd.pointCount;
        }
    }

    if (!currentBatch.commands.empty()) {
        batches_.push_back(std::move(currentBatch));
    }

    stats_.totalBatches = static_cast<uint32_t>(batches_.size());
    stats_.totalDrawCalls = stats_.totalBatches;
}

void RenderQueue::SortByPipeline() {
    sortByPipeline_ = true;
}

void RenderQueue::SortByDistance() {
    std::sort(pendingCommands_.begin(), pendingCommands_.end(),
              [](const RenderCommand& a, const RenderCommand& b) {
                  return a.distanceToCamera < b.distanceToCamera;
              });
    sortByPipeline_ = false;
}

void RenderQueue::SortByPipelineThenDistance() {
    sortByPipeline_ = true;
}

void RenderQueue::Clear() {
    pendingCommands_.clear();
    batches_.clear();
    stats_ = {};
}

void RenderQueue::Reserve(uint32_t estimatedCommands) {
    pendingCommands_.reserve(estimatedCommands);
}

} // namespace renderer
} // namespace workstation
