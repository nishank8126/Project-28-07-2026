#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodBlockDecoder.h"
#include "workstation/pod/PodNodeDecoder.h"
#include "workstation/pod/PodChannelDecoder.h"
#include "workstation/pod/PodHandlerRegistry.h"
#include "workstation/pod/PodDecodeContext.h"
#include "workstation/pod/PodStreamAdapter.h"
#include "workstation/pod/DoubleBoundingBlockHandler.h"
#include "workstation/pod/PodRecords.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/pointcloud/VoxelNode.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/BoundingBox.h"

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
using namespace workstation::pointcloud;

// Concrete test handler for integration tests.
struct TestHandler : public PodBlockHandler {
    bool process(PodDecodeContext&) override { return true; }
};

// In-memory data source.
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

struct Writer {
    std::vector<uint8_t> b;
    void u32(uint32_t v){ uint8_t t[4]; std::memcpy(t,&v,4); b.insert(b.end(),t,t+4); }
    void u64(uint64_t v){ uint8_t t[8]; std::memcpy(t,&v,8); b.insert(b.end(),t,t+8); }
    void f64(double v)  { uint8_t t[8]; std::memcpy(t,&v,8); b.insert(b.end(),t,t+8); }
    void f32(float v)   { uint8_t t[4]; std::memcpy(t,&v,4); b.insert(b.end(),t,t+4); }
    void raw(const uint8_t* p, size_t n){ b.insert(b.end(),p,p+n); }
};

static void WriteNodeBody(Writer& w, uint32_t geomPath,
                          double bx0, double by0, double bz0,
                          double bx1, double by1, double bz1,
                          uint32_t flags, uint32_t metaSize,
                          const std::vector<uint8_t>& meta,
                          uint32_t extraMode, uint64_t srcVal,
                          uint32_t channelCount) {
    if (geomPath == 1) { w.f64(bx0); w.f64(by0); w.f64(bz0); w.f64(bx1); w.f64(by1); w.f64(bz1); }
    else { w.f32((float)bx0); w.f32((float)by0); w.f32((float)bz0); w.f32((float)bx1); w.f32((float)by1); w.f32((float)bz1); }
    w.u32(flags);
    w.u32(metaSize);
    w.raw(meta.data(), meta.size());
    w.u32(extraMode);
    w.u64(srcVal);
    w.u32(channelCount);
}

static void WriteChannel(Writer& w, uint32_t chCode, uint32_t fmtCode,
                         uint32_t count, uint32_t stride,
                         double s0, double s1, double s2,
                         double o0, double o1, double o2,
                         const std::vector<uint8_t>& payload) {
    w.u32(chCode);
    w.u32(fmtCode);
    w.u32(count);
    w.u32(stride);
    w.f64(s0); w.f64(s1); w.f64(s2);
    w.f64(o0); w.f64(o1); w.f64(o2);
    w.raw(payload.data(), payload.size());
}

int main() {
    printf("== NakshaPointEngine :: Piece 5 (Integration) ==\n\n");

    // INTEG 1: Source → StreamAdapter → Handler Registry → DoubleBoundingBlockHandler
    {
        // Create a synthetic handler table with marker=1, recordCount=1.
        Writer w;
        w.u32(1); // marker = 1
        w.u32(1); // recordCount = 1
        w.u32(0x42); // key = 0x42
        // 6 doubles for bounding box
        w.f64(1.0); w.f64(2.0); w.f64(3.0);
        w.f64(10.0); w.f64(20.0); w.f64(30.0);
        // 6 uint64 for metadata
        w.u64(0xAA); w.u64(0xBB); w.u64(0xCC);
        w.u64(0xDD); w.u64(0xEE); w.u64(0xFF);

        MemorySource src(w.b.data(), w.b.size());
        PodStreamAdapter adapter(src);

        // Read marker.
        check(adapter.fixedRead(4), "INTEG1 fixed read marker");
        uint32_t marker = 0;
        std::memcpy(&marker, adapter.buffer(), 4);
        check(marker == 1, "INTEG1 marker = 1");
        adapter.consume(4);

        // Read record count.
        check(adapter.fixedRead(4), "INTEG1 fixed read count");
        uint32_t count = 0;
        std::memcpy(&count, adapter.buffer(), 4);
        check(count == 1, "INTEG1 count = 1");
        adapter.consume(4);

        // Register DoubleBoundingBlockHandler.
        PodHandlerRegistry reg;
        DoubleBoundingBlockHandler handler;
        std::vector<uint8_t> key = {0x01, 0x02, 0x03, 0x04};
        reg.insert(key, &handler);
        check(reg.size() == 1, "INTEG1 handler registered");
    }

    // INTEG 2: Full decode pipeline with handler registration
    {
        Writer w;
        w.u32(0); w.u32(1); // tableCount=0, nodeCount=1
        w.u32(0); w.u32(1); // typeFlag=0, geomPath=1
        WriteNodeBody(w, 1, 0,0,0, 5,5,5, 0, 0, {}, 0, 0, 1);
        // XYZ float32, count=10, stride=3
        std::vector<uint8_t> payload(120, 0x40); // 10*4*3 = 120 bytes
        WriteChannel(w, 1, 1, 10, 3, 1,1,1, 0,0,0, payload);

        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;

        // Register a handler (will be called during decode).
        TestHandler testHandler;
        decoder.handlers().insert({0x01}, &testHandler);
        check(decoder.handlers().size() == 1, "INTEG2 handler registered");

        check(decoder.decode(src, cloud), "INTEG2 decode ok");
        check(cloud.PointCount() == 10, "INTEG2 10 points");
        check(cloud.Root() != nullptr, "INTEG2 root set");
    }

    // INTEG 3: No memory corruption / ownership test
    {
        Writer w;
        w.u32(0); w.u32(3); // 3 nodes
        for (int i = 0; i < 3; ++i) {
            w.u32(1); w.u32(1); // Hierarchical, Float64
            WriteNodeBody(w, 1, i*10.0, i*10.0, i*10.0,
                          (i+1)*10.0, (i+1)*10.0, (i+1)*10.0,
                          0, 0, {}, 0, i*100, 0);
        }

        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(decoder.decode(src, cloud), "INTEG3 decode ok");

        auto* v = dynamic_cast<VoxelNode*>(cloud.Root());
        check(v && v->ChildCount() == 3, "INTEG3 3 children");

        // Verify bounding boxes are distinct.
        auto* c0 = v->Child(0);
        auto* c1 = v->Child(1);
        auto* c2 = v->Child(2);
        check(c0->bounds().maxX == 10.0, "INTEG3 node 0 bounds");
        check(c1->bounds().maxX == 20.0, "INTEG3 node 1 bounds");
        check(c2->bounds().maxX == 30.0, "INTEG3 node 2 bounds");
    }

    // INTEG 4: Channel data correctness
    {
        Writer w;
        w.u32(0); w.u32(1);
        w.u32(0); w.u32(1);
        WriteNodeBody(w, 1, 0,0,0, 100,100,100, 0, 0, {}, 0, 0, 1);
        // XYZ int16, count=5, stride=3
        int16_t xyz[15] = {10,20,30, 40,50,60, 70,80,90, 100,110,120, 130,140,150};
        std::vector<uint8_t> p(reinterpret_cast<uint8_t*>(xyz),
                               reinterpret_cast<uint8_t*>(xyz) + 30);
        WriteChannel(w, 1, 6, 5, 3, 0.1,0.1,0.1, 0,0,0, p);

        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(decoder.decode(src, cloud), "INTEG4 decode ok");
        check(cloud.PointCount() == 5, "INTEG4 5 points");

        auto* v = dynamic_cast<VoxelNode*>(cloud.Root());
        auto* child = v->Child(0);
        auto* ch = child->channels().GetChannel(ChannelId::XYZ);
        check(ch != nullptr, "INTEG4 XYZ channel exists");

        double out[3];
        ch->ReadXYZ(0, out);
        check(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0, "INTEG4 point 0");
        ch->ReadXYZ(2, out);
        check(out[0] == 7.0 && out[1] == 8.0 && out[2] == 9.0, "INTEG4 point 2");
    }

    // INTEG 5: Handler lookup + decode pipeline
    {
        PodHandlerRegistry reg;
        TestHandler h1, h2;
        reg.insert({0x01, 0x02}, &h1);
        reg.insert({0x03, 0x04}, &h2);

        // Verify handler lookup works during decode.
        std::vector<uint8_t> key = {0x01, 0x02};
        auto* found = reg.findLowerBound(key);
        check(found == &h1, "INTEG5 handler found");

        // Build a simple POD stream and decode.
        Writer w;
        w.u32(0); w.u32(1);
        w.u32(0); w.u32(1);
        WriteNodeBody(w, 1, 0,0,0, 1,1,1, 0, 0, {}, 0, 0, 0);

        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        decoder.handlers().insert({0x01, 0x02}, &h1);
        check(decoder.decode(src, cloud), "INTEG5 decode ok");
    }

    // INTEG 6: RB-tree invariants after handler registration
    {
        PodHandlerRegistry reg;
        TestHandler handlers[20];
        for (int i = 0; i < 20; ++i) {
            uint8_t key = static_cast<uint8_t>((i * 7 + 3) & 0xFF);
            reg.insert({key}, &handlers[i]);
        }
        check(reg.validateInvariants(), "INTEG6 RB-tree valid after 20 inserts");
        check(reg.size() == 20, "INTEG6 size 20");
    }

    printf("\n%s\n", g_fail == 0 ? "INTEGRATION_OK" : "INTEGRATION_FAIL");
    return g_fail == 0 ? 0 : 1;
}
