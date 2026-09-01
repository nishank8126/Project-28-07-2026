#pragma once
#include "workstation/gpu/PreparedGeometry.h"
#include "workstation/gpu/GPUBufferManager.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace workstation {
namespace gpu {

class GeometryCache {
public:
    bool Initialize(GPUBufferManager* bufferManager);
    void Shutdown();

    PreparedGeometry* PrepareNode(uint64_t nodeKey,
                                   const pointcloud::PointCloudNode* node);
    PreparedGeometry* GetGeometry(uint64_t nodeKey);
    void ReleaseGeometry(uint64_t nodeKey);

    void MarkDirty(uint64_t nodeKey);
    void EvictUnused(uint64_t currentFrame, uint64_t maxAge = 120);

    uint32_t GetCachedCount() const { return static_cast<uint32_t>(geometries_.size()); }
    uint64_t GetTotalGPUMemory() const;

    void PrepareCloud(pointcloud::PointCloud& cloud);

private:
    GPUBufferManager* bufferManager_ = nullptr;
    std::unordered_map<uint64_t, std::unique_ptr<PreparedGeometry>> geometries_;

    void ExtractNodeGeometry(PreparedGeometry& geo,
                              const pointcloud::PointCloudNode* node);
};

} // namespace gpu
} // namespace workstation
