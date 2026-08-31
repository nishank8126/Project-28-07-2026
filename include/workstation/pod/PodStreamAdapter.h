#pragma once
#include "workstation/pod/PodDataSource.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace workstation { namespace pod {

// Stream adapter with a 64 KiB working buffer (FUN_18007B940 evidence).
// Provides fixed-size and variable-size read operations.
// Clean-room design; no proprietary stream layout reproduced.
class PodStreamAdapter {
public:
    static constexpr std::size_t WORKING_BUFFER_SIZE = 0x10000; // 64 KiB

    explicit PodStreamAdapter(PodDataSource& source);
    ~PodStreamAdapter() = default;

    PodStreamAdapter(const PodStreamAdapter&) = delete;
    PodStreamAdapter& operator=(const PodStreamAdapter&) = delete;

    // Fixed-size read operation (strongly inferred: +0xA0).
    // Reads exactly `n` bytes into the working buffer.
    bool fixedRead(std::size_t n);

    // Variable-size byte transfer (strongly inferred: +0xA8).
    // Reads up to `maxBytes` into the working buffer.
    std::size_t transfer(std::size_t maxBytes);

    // Access the working buffer.
    const std::uint8_t* buffer() const { return buffer_.data(); }
    std::uint8_t* bufferMut() { return buffer_.data(); }
    std::size_t bufferOffset() const { return offset_; }
    std::size_t bufferedBytes() const { return end_; }

    // Reset the working buffer (call after consuming data).
    void consume(std::size_t bytes);

    // Total accumulated bytes transferred since last reset.
    std::uint64_t accumulated() const { return accumulated_; }

    // Position in the underlying source.
    std::uint64_t position() const { return source_.tell(); }

    // Access the underlying source.
    PodDataSource& source() { return source_; }

private:
    PodDataSource& source_;
    std::vector<std::uint8_t> buffer_;
    std::size_t offset_ = 0;
    std::size_t end_ = 0;
    std::uint64_t accumulated_ = 0;
};

}} // namespace workstation::pod
