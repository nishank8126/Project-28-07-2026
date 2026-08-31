#include "workstation/pod/PodBinaryReader.h"
#include <cstring>

namespace workstation { namespace pod {

PodBinaryReader::PodBinaryReader(PodDataSource& source, std::size_t bufferSize)
    : source_(source) {
    buffer_.resize(bufferSize > 0 ? bufferSize : DEFAULT_BUFFER_SIZE);
}

std::uint64_t PodBinaryReader::position() const {
    return baseOffset_ + pos_;
}

std::size_t PodBinaryReader::available() const {
    return end_ - pos_;
}

bool PodBinaryReader::read(void* out, std::size_t n) {
    if (n == 0) return true;
    while (available() < n) {
        if (!refill(n)) return false;
    }
    std::memcpy(out, &buffer_[pos_], n);
    pos_ += n;
    return true;
}

bool PodBinaryReader::readU32(std::uint32_t& v) { return read(&v, sizeof(v)); }
bool PodBinaryReader::readU64(std::uint64_t& v) { return read(&v, sizeof(v)); }
bool PodBinaryReader::readDouble(double& v)      { return read(&v, sizeof(v)); }
bool PodBinaryReader::readFloat(float& v)        { return read(&v, sizeof(v)); }

bool PodBinaryReader::refill(std::size_t needed) {
    // Preserve unread bytes at the front of the buffer.
    std::size_t rem = end_ - pos_;
    if (rem > 0) std::memmove(&buffer_[0], &buffer_[pos_], rem);
    baseOffset_ += pos_;
    pos_ = 0;
    end_ = rem;

    // Grow the window if it cannot hold the requested amount.
    if (buffer_.size() < needed) buffer_.resize(needed);

    // Read additional bytes from the source until we have `needed` or hit EOF.
    while (end_ < needed) {
        std::size_t want = buffer_.size() - end_;
        std::size_t got = source_.readSome(&buffer_[end_], want);
        if (got == 0) break;
        end_ += got;
    }
    return end_ >= needed;
}

} // namespace pod
} // namespace workstation
