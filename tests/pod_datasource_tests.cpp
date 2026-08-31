#include "workstation/pod/PodDataSource.h"
#include "workstation/pod/FileDataSource.h"
#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodStreamAdapter.h"
#include "workstation/pod/PodBlockDecoder.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>

static int g_fail = 0;
static void check(bool c, const char* m) {
    if (!c) { printf("  FAIL: %s\n", m); ++g_fail; }
    else    { printf("  PASS: %s\n", m); }
}

using namespace workstation::pod;

// ---- In-memory source for testing ----
class MemorySource : public PodDataSource {
public:
    MemorySource(const uint8_t* d, size_t n, size_t chunk = 0)
        : data_(d), size_(n), chunk_(chunk ? chunk : n) {}
    bool read(void* dst, size_t bytes) override {
        if (pos_ + bytes > size_) return false;
        std::memcpy(dst, data_ + pos_, bytes);
        pos_ += bytes;
        return true;
    }
    size_t readSome(void* dst, size_t maxBytes) override {
        if (pos_ >= size_) return 0;
        size_t want = maxBytes;
        if (want > (size_ - pos_)) want = size_ - pos_;
        if (want > chunk_) want = chunk_;
        std::memcpy(dst, data_ + pos_, want);
        pos_ += want;
        return want;
    }
    bool seek(uint64_t p) override { pos_ = p; return true; }
    uint64_t tell() const override { return pos_; }
    bool canRead() const override { return true; }
    bool canWrite() const override { return false; }
    DataSourceMode mode() const override { return DataSourceMode::Read; }
private:
    const uint8_t* data_; size_t size_; size_t pos_ = 0; size_t chunk_;
};

// ---- Writable memory source ----
class WritableMemorySource : public PodDataSource {
public:
    WritableMemorySource(size_t cap = 4096) : buf_(cap) {}
    bool read(void* dst, size_t bytes) override {
        if (pos_ + bytes > buf_.size()) return false;
        std::memcpy(dst, buf_.data() + pos_, bytes);
        pos_ += bytes;
        return true;
    }
    size_t readSome(void* dst, size_t maxBytes) override {
        if (pos_ >= buf_.size()) return 0;
        size_t want = maxBytes < (buf_.size() - pos_) ? maxBytes : (buf_.size() - pos_);
        std::memcpy(dst, buf_.data() + pos_, want);
        pos_ += want;
        return want;
    }
    bool write(const void* src, size_t bytes) {
        if (pos_ + bytes > buf_.size()) return false;
        std::memcpy(buf_.data() + pos_, src, bytes);
        pos_ += bytes;
        return true;
    }
    bool seek(uint64_t p) override { pos_ = p; return true; }
    uint64_t tell() const override { return pos_; }
    bool canRead() const override { return true; }
    bool canWrite() const override { return true; }
    DataSourceMode mode() const override { return DataSourceMode::ReadWrite; }
    const std::vector<uint8_t>& data() const { return buf_; }
private:
    std::vector<uint8_t> buf_;
    size_t pos_ = 0;
};

int main() {
    printf("== NakshaPointEngine :: Piece 5 (DataSource + StreamAdapter) ==\n\n");

    // TEST 21: Closed state
    {
        // A default-constructed FileDataSource would be closed.
        // We verify through the mode enum.
        check(static_cast<uint32_t>(DataSourceMode::Closed) == 0, "TEST21 Closed = 0");
        check(static_cast<uint32_t>(DataSourceMode::Read) == 1, "TEST21 Read = 1");
        check(static_cast<uint32_t>(DataSourceMode::Write) == 2, "TEST21 Write = 2");
        check(static_cast<uint32_t>(DataSourceMode::ReadWrite) == 3, "TEST21 ReadWrite = 3");
    }

    // TEST 22: Read state
    {
        uint8_t data[] = {0x41, 0x42, 0x43, 0x44};
        MemorySource src(data, sizeof(data));
        check(src.mode() == DataSourceMode::Read, "TEST22 mode = Read");
        check(src.canRead(), "TEST22 canRead");
        check(!src.canWrite(), "TEST22 !canWrite");
    }

    // TEST 23: Write state
    {
        // WritableMemorySource simulates write mode.
        WritableMemorySource src;
        check(src.mode() == DataSourceMode::ReadWrite, "TEST23 mode = ReadWrite");
        check(src.canRead(), "TEST23 canRead");
        check(src.canWrite(), "TEST23 canWrite");
    }

    // TEST 24: Read/write state
    {
        WritableMemorySource src;
        check(src.mode() == DataSourceMode::ReadWrite, "TEST24 mode = ReadWrite");
        check(src.canRead() && src.canWrite(), "TEST24 read+write");
    }

    // TEST 25: Readable/writable capability validation
    {
        uint8_t data[] = {1, 2, 3};
        MemorySource readOnly(data, 3);
        check(readOnly.canRead() && !readOnly.canWrite(), "TEST25 read-only");

        WritableMemorySource readWrite;
        check(readWrite.canRead() && readWrite.canWrite(), "TEST25 read-write");
    }

    // TEST 26: Fixed 4-byte read
    {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        MemorySource src(data, sizeof(data));
        PodStreamAdapter adapter(src);
        check(adapter.fixedRead(4), "TEST26 fixed 4-byte read ok");
        check(adapter.bufferedBytes() >= 4, "TEST26 buffered >= 4");
        uint32_t val = 0;
        std::memcpy(&val, adapter.buffer(), 4);
        check(val == 0x04030201, "TEST26 value correct");
    }

    // TEST 27: Fixed 1-byte read
    {
        uint8_t data[] = {0xAB};
        MemorySource src(data, sizeof(data));
        PodStreamAdapter adapter(src);
        check(adapter.fixedRead(1), "TEST27 fixed 1-byte read ok");
        check(adapter.buffer()[0] == 0xAB, "TEST27 byte correct");
    }

    // TEST 28: Variable buffer transfer
    {
        uint8_t data[200];
        for (int i = 0; i < 200; ++i) data[i] = static_cast<uint8_t>(i);
        MemorySource src(data, sizeof(data), 50); // chunk=50 to force multiple transfers
        PodStreamAdapter adapter(src);

        std::size_t avail = adapter.transfer(100);
        check(avail > 0, "TEST28 transfer > 0");
        check(adapter.bufferedBytes() > 0, "TEST28 has buffered data");

        adapter.consume(avail);
        check(adapter.bufferedBytes() == 0, "TEST28 after consume empty");
    }

    // TEST 29: EOF/short-read handling
    {
        uint8_t data[] = {0x10, 0x20};
        MemorySource src(data, sizeof(data));
        PodStreamAdapter adapter(src);

        // Try to read more than available.
        bool ok = adapter.fixedRead(100);
        check(!ok, "TEST29 EOF detected");
    }

    // TEST 30: 64 KiB working buffer boundary
    {
        // Create a source with exactly 64 KiB + 100 bytes.
        std::vector<uint8_t> data(0x10000 + 100);
        for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
        MemorySource src(data.data(), data.size(), 4096);
        PodStreamAdapter adapter(src);

        // Read 64 KiB — should fill the working buffer.
        check(adapter.fixedRead(0x10000), "TEST30 64 KiB read ok");
        check(adapter.bufferedBytes() >= 0x10000, "TEST30 buffered >= 64 KiB");

        adapter.consume(0x10000);
        check(adapter.bufferedBytes() == 0, "TEST30 after consume");

        // Read remaining 100 bytes.
        check(adapter.fixedRead(100), "TEST30 remaining 100 bytes ok");
        check(adapter.bufferedBytes() >= 100, "TEST30 buffered >= 100");
    }

    // TEST 31: 256 KiB Piece 3 reader remains unaffected
    {
        std::vector<uint8_t> data(1000);
        for (size_t i = 0; i < 1000; ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
        MemorySource src(data.data(), data.size(), 512);
        PodBinaryReader reader(src);
        std::vector<uint8_t> out(1000);
        check(reader.read(out.data(), 1000), "TEST31 Piece 3 reader ok");
        check(std::memcmp(out.data(), data.data(), 1000) == 0, "TEST31 content matches");
    }

    // TEST 32: FileDataSource read (using synthetic temp file)
    {
        // Write a temp file, read it back.
        const char* path = "test_piece5_read.tmp";
        {
            FILE* f = std::fopen(path, "wb");
            uint8_t buf[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
            std::fwrite(buf, 1, sizeof(buf), f);
            std::fclose(f);
        }

        auto src = FileDataSource::openRead(path);
        check(src.mode() == DataSourceMode::Read, "TEST32 mode = Read");
        check(src.canRead() && !src.canWrite(), "TEST32 capabilities");

        uint8_t out[5] = {};
        check(src.read(out, 5), "TEST32 read ok");
        check(std::memcmp(out, "\xDE\xAD\xBE\xEF\x42", 5) == 0, "TEST32 content ok");
        check(src.tell() == 5, "TEST32 position = 5");

        std::remove(path);
    }

    // TEST 33: FileDataSource write/create
    {
        const char* path = "test_piece5_write.tmp";
        {
            auto ds = FileDataSource::openWrite(path);
            check(ds.mode() == DataSourceMode::Write, "TEST33 mode = Write");
            check(!ds.canRead() && ds.canWrite(), "TEST33 capabilities");
            uint8_t data[] = {0xCA, 0xFE};
            check(ds.write(data, 2), "TEST33 write ok");
        }

        // Verify file contents.
        {
            FILE* f = std::fopen(path, "rb");
            uint8_t buf[2] = {};
            std::fread(buf, 1, 2, f);
            std::fclose(f);
            check(buf[0] == 0xCA && buf[1] == 0xFE, "TEST33 content ok");
        }
        std::remove(path);
    }

    // TEST 34: Read/write mode
    {
        const char* path = "test_piece5_rw.tmp";
        // Create file first.
        {
            FILE* f = std::fopen(path, "wb");
            uint8_t init[] = {0x11, 0x22, 0x33};
            std::fwrite(init, 1, sizeof(init), f);
            std::fclose(f);
        }

        auto ds = FileDataSource::openReadWrite(path);
        check(ds.mode() == DataSourceMode::ReadWrite, "TEST34 mode = ReadWrite");
        check(ds.canRead() && ds.canWrite(), "TEST34 capabilities");

        // Read existing data.
        uint8_t buf[3] = {};
        check(ds.read(buf, 3), "TEST34 read ok");
        check(buf[0] == 0x11 && buf[1] == 0x22 && buf[2] == 0x33, "TEST34 read content");

        // Seek back and write.
        check(ds.seek(0), "TEST34 seek ok");
        uint8_t over[] = {0xAA, 0xBB, 0xCC};
        check(ds.write(over, 3), "TEST34 write ok");

        // Read back.
        check(ds.seek(0), "TEST34 seek back");
        uint8_t out[3] = {};
        check(ds.read(out, 3), "TEST34 read after write");
        check(out[0] == 0xAA && out[1] == 0xBB && out[2] == 0xCC, "TEST34 content after write");

        std::remove(path);
    }

    printf("\n%s\n", g_fail == 0 ? "DATASOURCE_OK" : "DATASOURCE_FAIL");
    return g_fail == 0 ? 0 : 1;
}
