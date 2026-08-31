#include "workstation/pointcloud/PointCloudStreamReader.h"
#include <cstring>

namespace workstation { namespace pointcloud {

PointCloudStreamReader::PointCloudStreamReader(PointBlockSource& source, size_t bufferSize)
    : source_(source) {
    buffer_.resize(bufferSize > 0 ? bufferSize : DEFAULT_BUFFER_SIZE);
}

bool PointCloudStreamReader::Read(void* out, size_t n) {
    if (n == 0) return true;
    while (Available() < n) {
        if (!Refill(n)) return false;
    }
    std::memcpy(out, &buffer_[pos_], n);
    pos_ += n;
    return true;
}

bool PointCloudStreamReader::ReadU32(uint32_t& v) { return Read(&v, sizeof(v)); }
bool PointCloudStreamReader::ReadU64(uint64_t& v) { return Read(&v, sizeof(v)); }
bool PointCloudStreamReader::ReadDouble(double& v) { return Read(&v, sizeof(v)); }

bool PointCloudStreamReader::Refill(size_t needed) {
    // Preserve unread bytes at the front of the buffer.
    size_t rem = end_ - pos_;
    if (rem > 0) std::memmove(&buffer_[0], &buffer_[pos_], rem);
    baseOffset_ += pos_;
    pos_ = 0;
    end_ = rem;

    // Grow the window if it cannot hold the requested amount.
    if (buffer_.size() < needed) buffer_.resize(needed);

    // Read additional bytes from the source until we have `needed` or hit EOF.
    while (end_ < needed) {
        size_t want = buffer_.size() - end_;
        size_t got = source_.ReadSome(&buffer_[end_], want);
        if (got == 0) break;
        end_ += got;
    }
    return end_ >= needed;
}

} // namespace pointcloud
} // namespace workstation
