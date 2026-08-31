#pragma once
#include "workstation/pod/PodDataSource.h"
#include <cstdio>
#include <string>

namespace workstation { namespace pod {

// File-backed data source. Implements the observed open/read/write state machine
// (FUN_18007B940 evidence). Clean-room; no proprietary file handle layout.
class FileDataSource : public PodDataSource {
public:
    // Open an existing file for reading.
    static FileDataSource openRead(const std::string& path);

    // Create/open a file for writing.
    static FileDataSource openWrite(const std::string& path);

    // Open a file for read/write.
    static FileDataSource openReadWrite(const std::string& path);

    ~FileDataSource() override;

    FileDataSource(const FileDataSource&) = delete;
    FileDataSource& operator=(const FileDataSource&) = delete;

    FileDataSource(FileDataSource&& other) noexcept;
    FileDataSource& operator=(FileDataSource&& other) noexcept;

    bool read(void* destination, std::size_t bytes) override;
    std::size_t readSome(void* destination, std::size_t maxBytes) override;
    bool write(const void* source, std::size_t bytes);
    bool seek(std::uint64_t position) override;
    std::uint64_t tell() const override;

    bool canRead() const override;
    bool canWrite() const override;
    DataSourceMode mode() const override { return mode_; }

private:
    FileDataSource() = default;
    void close();

    std::FILE* file_ = nullptr;
    DataSourceMode mode_ = DataSourceMode::Closed;
    bool readable_ = false;
    bool writable_ = false;
};

}} // namespace workstation::pod
