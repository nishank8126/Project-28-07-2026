#include "workstation/pod/FileDataSource.h"
#include <algorithm>

namespace workstation { namespace pod {

FileDataSource::~FileDataSource() { close(); }

FileDataSource::FileDataSource(FileDataSource&& other) noexcept
    : file_(other.file_), mode_(other.mode_),
      readable_(other.readable_), writable_(other.writable_) {
    other.file_ = nullptr;
    other.mode_ = DataSourceMode::Closed;
    other.readable_ = false;
    other.writable_ = false;
}

FileDataSource& FileDataSource::operator=(FileDataSource&& other) noexcept {
    if (this != &other) {
        close();
        file_ = other.file_;
        mode_ = other.mode_;
        readable_ = other.readable_;
        writable_ = other.writable_;
        other.file_ = nullptr;
        other.mode_ = DataSourceMode::Closed;
        other.readable_ = false;
        other.writable_ = false;
    }
    return *this;
}

void FileDataSource::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    mode_ = DataSourceMode::Closed;
    readable_ = false;
    writable_ = false;
}

FileDataSource FileDataSource::openRead(const std::string& path) {
    FileDataSource ds;
    ds.file_ = std::fopen(path.c_str(), "rb");
    if (ds.file_) {
        ds.readable_ = true;
        ds.writable_ = false;
        ds.mode_ = DataSourceMode::Read;
    }
    return ds;
}

FileDataSource FileDataSource::openWrite(const std::string& path) {
    FileDataSource ds;
    ds.file_ = std::fopen(path.c_str(), "wb");
    if (ds.file_) {
        ds.readable_ = false;
        ds.writable_ = true;
        ds.mode_ = DataSourceMode::Write;
    }
    return ds;
}

FileDataSource FileDataSource::openReadWrite(const std::string& path) {
    FileDataSource ds;
    ds.file_ = std::fopen(path.c_str(), "r+b");
    if (!ds.file_) {
        // Fallback: create if not exists.
        ds.file_ = std::fopen(path.c_str(), "w+b");
    }
    if (ds.file_) {
        ds.readable_ = true;
        ds.writable_ = true;
        ds.mode_ = DataSourceMode::ReadWrite;
    }
    return ds;
}

bool FileDataSource::read(void* destination, std::size_t bytes) {
    if (!file_ || !readable_) return false;
    if (bytes == 0) return true;
    std::size_t got = std::fread(destination, 1, bytes, file_);
    return got == bytes;
}

std::size_t FileDataSource::readSome(void* destination, std::size_t maxBytes) {
    if (!file_ || !readable_) return 0;
    if (maxBytes == 0) return 0;
    return std::fread(destination, 1, maxBytes, file_);
}

bool FileDataSource::write(const void* source, std::size_t bytes) {
    if (!file_ || !writable_) return false;
    if (bytes == 0) return true;
    std::size_t written = std::fwrite(source, 1, bytes, file_);
    return written == bytes;
}

bool FileDataSource::seek(std::uint64_t position) {
    if (!file_) return false;
#if defined(_WIN32)
    return _fseeki64(file_, static_cast<long long>(position), SEEK_SET) == 0;
#else
    return std::fseeko(file_, static_cast<off_t>(position), SEEK_SET) == 0;
#endif
}

std::uint64_t FileDataSource::tell() const {
    if (!file_) return 0;
#if defined(_WIN32)
    return static_cast<std::uint64_t>(_ftelli64(file_));
#else
    return static_cast<std::uint64_t>(std::ftello(file_));
#endif
}

bool FileDataSource::canRead() const { return file_ && readable_; }
bool FileDataSource::canWrite() const { return file_ && writable_; }

}} // namespace workstation::pod
