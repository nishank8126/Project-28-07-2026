#pragma once
#include <cstddef>
#include <cstdint>

namespace workstation { namespace pod {

// State modes observed in the original (FUN_18007B940 evidence).
enum class DataSourceMode : std::uint32_t {
    Closed = 0,
    Read   = 1,
    Write  = 2,
    ReadWrite = 3
};

// Abstract data source for POD file I/O. Provides read/seek/tell.
// Independent C++ design; no proprietary class names or ABI.
class PodDataSource {
public:
    virtual ~PodDataSource() = default;

    // Read exactly `bytes` into destination. Returns false on failure/EOF.
    virtual bool read(void* destination, std::size_t bytes) = 0;

    // Read up to `maxBytes` into destination; returns bytes actually read (0 = EOF).
    // Used by the buffered reader for efficient refills.
    virtual std::size_t readSome(void* destination, std::size_t maxBytes) = 0;

    // Seek to an absolute byte position. Returns false if not seekable.
    virtual bool seek(std::uint64_t position) = 0;

    // Return the current byte position.
    virtual std::uint64_t tell() const = 0;

    // Capability queries.
    virtual bool canRead() const = 0;
    virtual bool canWrite() const = 0;

    // Mode query.
    virtual DataSourceMode mode() const = 0;
};

}} // namespace workstation::pod
