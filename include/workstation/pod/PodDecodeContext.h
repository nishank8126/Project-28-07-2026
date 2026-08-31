#pragma once
#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pointcloud/PointCloud.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace workstation { namespace pod {

// Decode context passed to handlers during block processing.
// Contains references to the reader, cloud, and handler-specific state.
// Clean-room design; no proprietary context layout reproduced.
class PodDecodeContext {
public:
    PodDecodeContext(PodBinaryReader& reader, pointcloud::PointCloud& cloud)
        : reader_(reader), cloud_(cloud) {}

    PodBinaryReader& reader() { return reader_; }
    pointcloud::PointCloud& cloud() { return cloud_; }

    // Handler-specific metadata (set by concrete handlers).
    void setMetadata(std::uint64_t key, std::uint64_t value) {
        metadata_[key] = value;
    }

    std::uint64_t getMetadata(std::uint64_t key) const {
        auto it = metadata_.find(key);
        return (it != metadata_.end()) ? it->second : 0;
    }

private:
    PodBinaryReader& reader_;
    pointcloud::PointCloud& cloud_;
    std::unordered_map<std::uint64_t, std::uint64_t> metadata_;
};

}} // namespace workstation::pod
