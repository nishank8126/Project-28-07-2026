#pragma once
#include "workstation/pod/PodDataSource.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace workstation { namespace pod {

// Buffered binary reader wrapping a PodDataSource. Implements the confirmed
// 256 KB (0x40000) refill algorithm (FUN_18006d760). Independent C++ design.
class PodBinaryReader {
public:
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 0x40000; // 256 KB confirmed

    explicit PodBinaryReader(PodDataSource& source,
                             std::size_t bufferSize = DEFAULT_BUFFER_SIZE);

    // Read exactly n bytes. Returns false on end-of-stream or error.
    bool read(void* out, std::size_t n);
    bool readU32(std::uint32_t& v);
    bool readU64(std::uint64_t& v);
    bool readDouble(double& v);
    bool readFloat(float& v);

    std::uint64_t position() const;
    std::size_t available() const;

private:
    bool refill(std::size_t needed);

    PodDataSource& source_;
    std::vector<std::uint8_t> buffer_;
    std::size_t pos_ = 0;
    std::size_t end_ = 0;
    std::uint64_t baseOffset_ = 0;
};

} // namespace pod
} // namespace workstation
