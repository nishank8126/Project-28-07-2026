#include "workstation/pod/PodBinaryReader.h"
#include "workstation/pod/PodBlockDecoder.h"
#include "workstation/pod/PodNodeDecoder.h"
#include "workstation/pod/PodChannelDecoder.h"
#include "workstation/pod/PodHandlerRegistry.h"
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

// Concrete test handler for registry tests (PodBlockHandler is abstract).
struct TestBlockHandler : public workstation::pod::PodBlockHandler {
    bool process(workstation::pod::PodDecodeContext&) override { return true; }
};

// ---- Test helpers ----

class MemorySource : public workstation::pod::PodDataSource {
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
    workstation::pod::DataSourceMode mode() const override {
        return workstation::pod::DataSourceMode::Read;
    }
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

using namespace workstation::pod;
using namespace workstation::pointcloud;

// Write the POD node body (geometry onwards). typeFlag and geomPath are NOT
// written here because DecodeNode receives them as parameters; PodBlockDecoder
// reads them from the stream before calling DecodeNode.
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
    printf("== NakshaPointEngine :: Piece 4 (POD Block Decoder) ==\n\n");

    // TEST 1: 256 KB reader sequential read
    {
        std::vector<uint8_t> data(2000);
        for (size_t i = 0; i < 2000; ++i) data[i] = (uint8_t)(i & 0xFF);
        MemorySource src(data.data(), data.size(), 4096);
        PodBinaryReader r(src);
        std::vector<uint8_t> out(2000);
        check(r.read(out.data(), 2000), "TEST1 read 2000 bytes");
        check(std::memcmp(out.data(), data.data(), 2000) == 0, "TEST1 content matches");
        check(r.position() == 2000, "TEST1 position advanced");
    }

    // TEST 2: Refill across 256 KB boundary
    {
        std::vector<uint8_t> data(500);
        for (size_t i = 0; i < 500; ++i) data[i] = (uint8_t)((i * 3) & 0xFF);
        MemorySource src(data.data(), data.size(), 7);
        PodBinaryReader r(src);
        std::vector<uint8_t> out(500);
        check(r.read(out.data(), 500), "TEST2 read across refills");
        check(std::memcmp(out.data(), data.data(), 500) == 0, "TEST2 refill content matches");
    }

    // TEST 3: Exact type normalization (1..10, invalid)
    {
        bool ok = true;
        for (uint32_t i = 1; i <= 10; ++i)
            if (NormalizeFormatCode(i) != i) ok = false;
        if (NormalizeFormatCode(0) != 0) ok = false;
        if (NormalizeFormatCode(11) != 0) ok = false;
        if (NormalizeFormatCode(999) != 0) ok = false;
        check(ok, "TEST3 format code normalization");
    }

    // TEST 4: Exact element-size table
    {
        check(GetElementSize(1) == 4,  "TEST4 Type1 = 4");
        check(GetElementSize(2) == 8,  "TEST4 Type2 = 8");
        check(GetElementSize(3) == 0,  "TEST4 Type3 = 0");
        check(GetElementSize(4) == 1,  "TEST4 Type4 = 1");
        check(GetElementSize(5) == 1,  "TEST4 Type5 = 1");
        check(GetElementSize(6) == 2,  "TEST4 Type6 = 2");
        check(GetElementSize(7) == 2,  "TEST4 Type7 = 2");
        check(GetElementSize(8) == 4,  "TEST4 Type8 = 4");
        check(GetElementSize(9) == 4,  "TEST4 Type9 = 4");
        check(GetElementSize(10) == 8, "TEST4 Type10 = 8");
        check(GetElementSize(0) == 0,  "TEST4 invalid = 0");
        check(GetElementSize(11) == 0, "TEST4 invalid = 0");
    }

    // TEST 5: Channel-code normalization (1..7, invalid)
    {
        bool ok = true;
        for (uint32_t i = 1; i <= 7; ++i)
            if (NormalizeChannelCode(i) != i) ok = false;
        if (NormalizeChannelCode(0) != 0) ok = false;
        if (NormalizeChannelCode(8) != 0) ok = false;
        if (NormalizeChannelCode(99) != 0) ok = false;
        check(ok, "TEST5 channel code normalization");
    }

    // TEST 6: Normal node creation
    {
        Writer w;
        WriteNodeBody(w, 1, 0,0,0, 10,10,10, 0, 0, {}, 0, 0xABCD, 0);
        MemorySource src(w.b.data(), w.b.size());
        PodBinaryReader reader(src);
        uint64_t srcVal = 0; NodeExtraMetadata meta;
        auto node = PodNodeDecoder::DecodeNode(reader, NodeType::Normal,
                                                GeometryPath::Float64, srcVal, meta);
        check(node != nullptr, "TEST6 normal node created");
        check(!node->IsVoxel(), "TEST6 not a voxel");
        check(node->bounds().maxX == 10.0, "TEST6 bounds set");
        check(srcVal == 0xABCD, "TEST6 source node value");
    }

    // TEST 7: Voxel node creation
    {
        Writer w;
        WriteNodeBody(w, 1, 0,0,0, 5,5,5, 0, 0, {}, 0, 0x1234, 0);
        MemorySource src(w.b.data(), w.b.size());
        PodBinaryReader reader(src);
        uint64_t srcVal = 0; NodeExtraMetadata meta;
        auto node = PodNodeDecoder::DecodeNode(reader, NodeType::Hierarchical,
                                                GeometryPath::Float64, srcVal, meta);
        check(node != nullptr, "TEST7 voxel node created");
        check(node->IsVoxel(), "TEST7 is a voxel");
        check(dynamic_cast<VoxelNode*>(node.get())->density() == 1.0, "TEST7 density = 1.0");
    }

    // TEST 8: Float geometry path
    {
        Writer w;
        WriteNodeBody(w, 0, 1.5, 2.5, 3.5, 6.5, 7.5, 8.5, 0, 0, {}, 0, 0, 0);
        MemorySource src(w.b.data(), w.b.size());
        PodBinaryReader reader(src);
        uint64_t srcVal = 0; NodeExtraMetadata meta;
        auto node = PodNodeDecoder::DecodeNode(reader, NodeType::Normal,
                                                GeometryPath::Float32, srcVal, meta);
        check(node != nullptr, "TEST8 float path ok");
        check(node->bounds().minX == 1.5 && node->bounds().maxX == 6.5, "TEST8 float→double bounds");
    }

    // TEST 9: Double geometry path
    {
        Writer w;
        WriteNodeBody(w, 1, 1.1, 2.2, 3.3, 9.9, 8.8, 7.7, 0, 0, {}, 0, 0, 0);
        MemorySource src(w.b.data(), w.b.size());
        PodBinaryReader reader(src);
        uint64_t srcVal = 0; NodeExtraMetadata meta;
        auto node = PodNodeDecoder::DecodeNode(reader, NodeType::Normal,
                                                GeometryPath::Float64, srcVal, meta);
        check(node != nullptr, "TEST9 double path ok");
        check(node->bounds().minX == 1.1 && node->bounds().maxZ == 7.7, "TEST9 double bounds");
    }

    // TEST 10: Payload byte calculation
    {
        size_t sz;
        check(CalculatePayloadSize(1, 100, 3, sz) && sz == 1200, "TEST10 Type1 payload");
        check(CalculatePayloadSize(6, 50, 3, sz) && sz == 300, "TEST10 Type6 payload");
        check(CalculatePayloadSize(3, 100, 1, sz) && sz == 0, "TEST10 Type3 payload=0");
        // overflow check
        check(!CalculatePayloadSize(1, UINT32_MAX, UINT32_MAX, sz), "TEST10 overflow detected");
    }

    // TEST 11: Existing channel replacement (via PointStorage::AddChannel)
    {
        PointStorage ps;
        uint8_t buf[3] = {1,2,3};
        PointAttributeChannel ch1 = CreateChannel(ChannelId::RGB, PointFormat::UInt8, 1, buf);
        PointAttributeChannel ch2 = CreateChannel(ChannelId::RGB, PointFormat::UInt8, 1, buf);
        ps.AddChannel(std::move(ch1));
        check(ps.GetChannel(ChannelId::RGB) != nullptr, "TEST11 first channel added");
        ps.AddChannel(std::move(ch2));
        check(ps.GetChannel(ChannelId::RGB) != nullptr, "TEST11 channel replaced");
        check(ps.ChannelCount() == 1, "TEST11 no duplicate");
    }

    // TEST 12: New channel creation
    {
        int16_t xyz[6] = {100,200,300, 400,500,600};
        double s[3] = {0.01,0.01,0.01}; double o[3] = {0,0,0};
        auto ch = CreateChannel(ChannelId::XYZ, PointFormat::Int16, 2, xyz, s, o, 6);
        check(ch.Count() == 2, "TEST12 count=2");
        double out[3];
        ch.ReadXYZ(0, out);
        check(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0, "TEST12 int16 decode");
    }

    // TEST 13: Node attachment to cloud
    {
        auto node = CreatePointNode(BoundingBox{0,0,0,1,1,1});
        uint8_t rgb[3] = {10,20,30};
        node->channels().AddChannel(CreateChannel(ChannelId::RGB, PointFormat::UInt8, 1, rgb));
        auto root = CreateVoxelNode(BoundingBox{});
        root->AddChild(std::move(node));
        PointCloud cloud;
        cloud.SetRoot(root.get());
        cloud.Finalize();
        check(cloud.Root() != nullptr, "TEST13 root set");
        auto* v = dynamic_cast<VoxelNode*>(cloud.Root());
        check(v && v->ChildCount() == 1, "TEST13 one child");
    }

    // TEST 14: FinalizeCloud aggregation
    {
        auto root = CreateVoxelNode(BoundingBox{});
        auto leaf = CreatePointNode(BoundingBox{0,0,0,1,1,1});
        int16_t xyz[300]; for (int i=0;i<300;i++) xyz[i]=100;
        leaf->channels().AddChannel(
            CreateChannel(ChannelId::XYZ, PointFormat::Int16, 100, xyz));
        root->AddChild(std::move(leaf));
        PointCloud cloud;
        cloud.SetRoot(root.get());
        cloud.Finalize();
        check(cloud.PointCount() == 100, "TEST14 point count");
        check(cloud.Attributes().Has(PointAttribute::XYZ), "TEST14 XYZ attribute");
    }

    // TEST 15: Handler registry lower-bound search
    {
        PodHandlerRegistry reg;
        TestBlockHandler h1, h2, h3;
        reg.insert({0x01, 0x02}, &h1);
        reg.insert({0x01, 0x03}, &h2);
        reg.insert({0x02, 0x01}, &h3);
        std::vector<uint8_t> key = {0x01, 0x02};
        check(reg.findLowerBound(key) == &h1, "TEST15 exact match");
        std::vector<uint8_t> key2 = {0x01, 0x02, 0x00};
        check(reg.findLowerBound(key2) == &h2, "TEST15 lower bound past match");
        std::vector<uint8_t> key3 = {0x00};
        check(reg.findLowerBound(key3) == &h1, "TEST15 lower bound before all");
        std::vector<uint8_t> key4 = {0xFF};
        check(reg.findLowerBound(key4) == nullptr, "TEST15 lower bound past all");
    }

    // TEST 16: Handler-key comparison semantics (via public findLowerBound)
    {
        // a=01 02, b=01 03, c=01 02 00, d=01
        TestBlockHandler ha, hb, hc, hd;
        PodHandlerRegistry reg;
        reg.insert({0x01, 0x02}, &ha);
        reg.insert({0x01, 0x03}, &hb);
        reg.insert({0x01, 0x02, 0x00}, &hc);
        reg.insert({0x01}, &hd);
        // a < b: search for b returns hb (b is not less than itself; lower_bound skips to hb or past)
        std::vector<uint8_t> kb = {0x01, 0x03};
        check(reg.findLowerBound(kb) == &hb, "TEST16 a < b via lower bound");
        // search for a returns ha
        std::vector<uint8_t> ka = {0x01, 0x02};
        check(reg.findLowerBound(ka) == &ha, "TEST16 a = a via lower bound");
        // search for c (01 02 00) — exact match is hc
        std::vector<uint8_t> kc = {0x01, 0x02, 0x00};
        check(reg.findLowerBound(kc) == &hc, "TEST16 c = c via lower bound");
        // search for d (01) — d is less than all, so lower bound is hd
        std::vector<uint8_t> kd = {0x01};
        check(reg.findLowerBound(kd) == &hd, "TEST16 d via lower bound");
    }

    // TEST 17: Multiple nodes
    {
        Writer w;
        w.u32(0); w.u32(2); // tableCount=0, nodeCount=2
        w.u32(0); w.u32(1); WriteNodeBody(w, 1, 0,0,0, 1,1,1, 0, 0, {}, 0, 100, 0);
        w.u32(0); w.u32(1); WriteNodeBody(w, 1, 1,1,1, 2,2,2, 0, 0, {}, 0, 200, 0);
        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(decoder.decode(src, cloud), "TEST17 decode ok");
        auto* v = dynamic_cast<VoxelNode*>(cloud.Root());
        check(v && v->ChildCount() == 2, "TEST17 two nodes");
        check(cloud.PointCount() == 0, "TEST17 no XYZ channels");
    }

    // TEST 18: Multiple channels
    {
        Writer w;
        w.u32(0); w.u32(1); // 1 node
        w.u32(0); w.u32(1); WriteNodeBody(w, 1, 0,0,0, 5,5,5, 0, 0, {}, 0, 0, 2);
        // channel 1: XYZ int16, count=3, stride=3 → payload = 2*3*3=18 bytes
        std::vector<uint8_t> p1(18, 0x64); // fill with 100
        WriteChannel(w, 1, 6, 3, 3, 0.01,0.01,0.01, 0,0,0, p1);
        // channel 2: RGB uint8, count=3, stride=3 → payload = 1*3*3=9 bytes
        std::vector<uint8_t> p2(9, 0x80);
        WriteChannel(w, 2, 4, 3, 3, 1,1,1, 0,0,0, p2);
        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(decoder.decode(src, cloud), "TEST18 decode ok");
        auto* v = dynamic_cast<VoxelNode*>(cloud.Root());
        auto* child = v->Child(0);
        check(child->channels().GetChannel(ChannelId::XYZ) != nullptr, "TEST18 XYZ channel");
        check(child->channels().GetChannel(ChannelId::RGB) != nullptr, "TEST18 RGB channel");
    }

    // TEST 19: Truncated/corrupt stream detection
    {
        // Truncated stream: tableCount present but nodeCount missing.
        Writer w;
        w.u32(0); // tableCount
        // missing nodeCount
        MemorySource src(w.b.data(), w.b.size());
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(!decoder.decode(src, cloud), "TEST19 truncated stream rejected");
    }

    // TEST 20: Large payload requiring refill
    {
        Writer w;
        w.u32(0); w.u32(1);
        // node with 1 XYZ float channel: count=500, stride=3, payload=4*500*3=6000 bytes
        w.u32(0); w.u32(1); WriteNodeBody(w, 1, 0,0,0, 10,10,10, 0, 0, {}, 0, 0, 1);
        std::vector<uint8_t> payload(6000, 0x42);
        WriteChannel(w, 1, 1, 500, 3, 1,1,1, 0,0,0, payload);
        // Use small chunk to force refills.
        MemorySource src(w.b.data(), w.b.size(), 256);
        PodBlockDecoder decoder;
        PointCloud cloud;
        check(decoder.decode(src, cloud), "TEST20 large payload ok");
        check(cloud.PointCount() == 500, "TEST20 500 points decoded");
    }

    printf("\n%s\n", g_fail == 0 ? "PIECE4_OK" : "PIECE4_FAIL");
    return g_fail == 0 ? 0 : 1;
}
