#include "workstation/gpu/GPUBufferManager.h"

namespace workstation {
namespace gpu {

bool GPUBufferManager::Initialize(vulkan::VulkanAllocator& allocator, uint32_t maxFramesInFlight) {
    allocator_ = &allocator;
    maxFramesInFlight_ = maxFramesInFlight;
    return true;
}

void GPUBufferManager::Shutdown() {
    for (auto& a : stagingAllocations_) {
        if (a && a->IsValid()) {
            allocator_->DestroyBuffer(*reinterpret_cast<vulkan::GPUBuffer*>(a.get()));
        }
    }
    stagingAllocations_.clear();
    for (auto& a : allocations_) {
        if (a && a->IsValid()) {
            allocator_->DestroyBuffer(*reinterpret_cast<vulkan::GPUBuffer*>(a.get()));
        }
    }
    allocations_.clear();
    allocator_ = nullptr;
}

GPUBufferAllocation* GPUBufferManager::Allocate(BufferType type, VkDeviceSize size, bool hostVisible) {
    auto alloc = std::make_unique<GPUBufferAllocation>();
    alloc->type = type;
    alloc->size = size;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (type == BufferType::Uniform) {
        usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    } else if (type == BufferType::IndirectDraw) {
        usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    VmaMemoryUsage memUsage = hostVisible ? VMA_MEMORY_USAGE_AUTO_PREFER_HOST
                                           : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VmaAllocationCreateFlags flags = hostVisible ?
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memUsage;
    allocInfo.flags = flags;

    vmaCreateBuffer(allocator_->GetAllocator(), &bufferInfo, &allocInfo,
                    &alloc->buffer, &alloc->allocation, &alloc->info);

    GPUBufferAllocation* ptr = alloc.get();
    std::lock_guard<std::mutex> lock(mutex_);
    allocations_.push_back(std::move(alloc));
    return ptr;
}

void GPUBufferManager::Free(GPUBufferAllocation* allocation) {
    if (!allocation) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(allocations_.begin(), allocations_.end(),
                           [allocation](const std::unique_ptr<GPUBufferAllocation>& a) {
                               return a.get() == allocation;
                           });
    if (it != allocations_.end()) {
        vmaDestroyBuffer(allocator_->GetAllocator(), (*it)->buffer, (*it)->allocation);
        allocations_.erase(it);
    }
}

GPUBufferAllocation* GPUBufferManager::AllocateStaging(VkDeviceSize size) {
    return Allocate(BufferType::Staging, size, true);
}

void GPUBufferManager::FreeStaging(GPUBufferAllocation* staging) {
    Free(staging);
}

void GPUBufferManager::UploadToGPU(GPUBufferAllocation* dst, const void* data,
                                     VkDeviceSize size, VkDeviceSize offset) {
    if (!dst || !data || !dst->info.pMappedData) return;
    memcpy(static_cast<uint8_t*>(dst->info.pMappedData) + offset, data, size);
    vmaFlushAllocation(allocator_->GetAllocator(), dst->allocation, offset, size);
}

void GPUBufferManager::EvictUnused(uint64_t currentFrame, uint64_t maxAge) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = allocations_.begin();
    while (it != allocations_.end()) {
        if (!(*it)->isPersistent &&
            currentFrame - (*it)->lastUsedFrame > maxAge) {
            vmaDestroyBuffer(allocator_->GetAllocator(), (*it)->buffer, (*it)->allocation);
            it = allocations_.erase(it);
        } else {
            ++it;
        }
    }
}

GPUBufferManager::Stats GPUBufferManager::GetStats() const {
    Stats stats{};
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& a : allocations_) {
        stats.totalAllocated += a->size;
        stats.allocationCount++;
        switch (a->type) {
            case BufferType::PointPosition:
            case BufferType::PointColor:
            case BufferType::PointIntensity:
            case BufferType::PointClassification:
            case BufferType::PointNormal:
                stats.pointBufferCount++;
                break;
            case BufferType::Uniform:
                stats.uniformBufferCount++;
                break;
            case BufferType::IndirectDraw:
                stats.indirectBufferCount++;
                break;
            default:
                break;
        }
    }
    return stats;
}

} // namespace gpu
} // namespace workstation
