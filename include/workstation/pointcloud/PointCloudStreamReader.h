#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace workstation { namespace pointcloud {

// Abstract source the buffered reader pulls bytes from. Independent of any
// concrete file/network format (no LAZ/POD reader here).
class PointBlockSource {
public:
    virtual ~PointBlockSource() = default;
    // Read up to maxBytes into buf; return the number of bytes actually read
    // (0 indicates end-of-stream).
    virtual size_t ReadSome(void* buf, size_t maxBytes) = 0;
};

// Buffered stream reader (FUN_18006d760). Refills a 256 KB window from the
// source, preserving unread bytes across refills. Independent C++ design.
class PointCloudStreamReader {
public:
    static constexpr size_t DEFAULT_BUFFER_SIZE = 0x40000; // 256 KB (confirmed)

    PointCloudStreamReader(PointBlockSource& source, size_t bufferSize = DEFAULT_BUFFER_SIZE);

    // Read exactly n bytes (refilling as needed). False on end-of-stream.
    bool Read(void* out, size_t n);
    bool ReadU32(uint32_t& v);
    bool ReadU64(uint64_t& v);
    bool ReadDouble(double& v);

    size_t Position() const { return baseOffset_ + pos_; }
    size_t Available() const { return end_ - pos_; }

private:
    bool Refill(size_t needed);

    PointBlockSource& source_;
    std::vector<uint8_t> buffer_;
    size_t pos_ = 0;     // read cursor within buffer_
    size_t end_ = 0;     // valid bytes in buffer_
    size_t baseOffset_ = 0; // absolute offset of buffer_[0]
};

} // namespace pointcloud
} // namespace workstation
