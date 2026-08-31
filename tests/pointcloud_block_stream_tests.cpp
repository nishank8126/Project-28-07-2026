#include "workstation/pointcloud/PointCloudStreamReader.h"
#include "workstation/pointcloud/PointBlockParser.h"
#include "workstation/pointcloud/CloudBlockLoader.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include "workstation/pointcloud/BoundingBox.h"
#include "workstation/pointcloud/VoxelNode.h"

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

// In-memory source that returns at most `chunk` bytes per ReadSome (to force
// stream refills).
class MemoryBlockSource : public workstation::pointcloud::PointBlockSource {
public:
    MemoryBlockSource(const uint8_t* d, size_t n, size_t chunk)
        : data_(d), size_(n), chunk_(chunk) {}
    size_t ReadSome(void* buf, size_t maxBytes) override {
        if (pos_ >= size_) return 0;
        size_t want = maxBytes;
        if (want > (size_ - pos_)) want = size_ - pos_;
        if (want > chunk_) want = chunk_;
        std::memcpy(buf, data_ + pos_, want);
        pos_ += want;
        return want;
    }
private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    size_t chunk_;
};

// Little-endian binary writer (native on x86-64 test host).
struct BinWriter {
    std::vector<uint8_t> b;
    void u32(uint32_t v) { uint8_t t[4]; std::memcpy(t, &v, 4); b.insert(b.end(), t, t + 4); }
    void f64(double v)   { uint8_t t[8]; std::memcpy(t, &v, 8); b.insert(b.end(), t, t + 8); }
    void raw(const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); }
};

static workstation::pointcloud::BoundingBox Box(double lo, double hi) {
    workstation::pointcloud::BoundingBox bb;
    bb.minX = bb.minY = bb.minZ = lo;
    bb.maxX = bb.maxY = bb.maxZ = hi;
    return bb;
}

// Append an XYZ-float channel (count*3 floats) into a writer.
static void AppendXYZFloat(BinWriter& w, uint32_t count, const std::vector<float>& vals) {
    w.u32(0);                 // id XYZ
    w.u32(0);                 // format Float32
    w.u32(count);
    w.u32(12);                // stride (per-element: 3 * float32)
    w.f64(1); w.f64(1); w.f64(1);
    w.f64(0); w.f64(0); w.f64(0);
    w.raw(reinterpret_cast<const uint8_t*>(vals.data()), vals.size() * sizeof(float));
}

int main() {
    using namespace workstation::pointcloud;
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("== NakshaPointEngine :: Piece 3 (Block Streaming & Scene Population) ==\n\n");

    // TEST 1: stream reader reads sequential bytes
    {
        std::vector<uint8_t> data(1000);
        for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);
        MemoryBlockSource src(data.data(), data.size(), 4096);
        PointCloudStreamReader r(src);
        std::vector<uint8_t> out(1000);
        check(r.Read(out.data(), 1000), "TEST1 read 1000 bytes");
        check(std::memcmp(out.data(), data.data(), 1000) == 0, "TEST1 content matches");
        check(r.Position() == 1000, "TEST1 position advanced");
    }

    // TEST 2: stream reader handles buffer refill
    {
        std::vector<uint8_t> data(500);
        for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>((i * 7) & 0xFF);
        MemoryBlockSource src(data.data(), data.size(), 7); // tiny chunk -> many refills
        PointCloudStreamReader r(src);
        std::vector<uint8_t> out(500);
        check(r.Read(out.data(), 500), "TEST2 read across refills");
        check(std::memcmp(out.data(), data.data(), 500) == 0, "TEST2 refill content matches");

        // u32 spanning refill boundaries (1 byte per ReadSome)
        BinWriter w; w.u32(0x12345678);
        MemoryBlockSource src2(w.b.data(), w.b.size(), 1);
        PointCloudStreamReader r2(src2);
        uint32_t v = 0;
        check(r2.ReadU32(v), "TEST2 read u32 across refill");
        check(v == 0x12345678, "TEST2 u32 value correct");
    }

    // TEST 3: Node descriptor creates PointCloudNode
    {
        BinWriter w;
        w.u32(0);                 // type Normal
        w.f64(0); w.f64(0); w.f64(0); w.f64(10); w.f64(10); w.f64(10); // bounds
        w.u32(0);                 // flags
        w.u32(0);                 // channelCount
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        PointCloudStreamReader r(src);
        bool ok = false;
        auto node = PointBlockParser::ParseNode(r, ok);
        check(ok && node != nullptr, "TEST3 parse normal node ok");
        check(!node->IsVoxel(), "TEST3 not a voxel");
        check(node->bounds().maxX == 10.0, "TEST3 bounds set");
    }

    // TEST 4: Voxel descriptor creates VoxelNode
    {
        BinWriter w;
        w.u32(1);                 // type Voxel
        w.f64(0); w.f64(0); w.f64(0); w.f64(10); w.f64(10); w.f64(10);
        w.u32(0);
        w.u32(0);
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        PointCloudStreamReader r(src);
        bool ok = false;
        auto node = PointBlockParser::ParseNode(r, ok);
        check(ok && node != nullptr, "TEST4 parse voxel node ok");
        check(node->IsVoxel(), "TEST4 is a voxel");
        check(static_cast<VoxelNode*>(node.get())->density() == 1.0, "TEST4 density = 1.0");
    }

    // TEST 5: Channel descriptor creates channel
    {
        BinWriter w;
        w.u32(0);                 // id XYZ
        w.u32(1);                 // format Int16
        w.u32(2);                 // count
        w.u32(6);                 // stride (3 * int16)
        w.f64(0.01); w.f64(0.01); w.f64(0.01);
        w.f64(0); w.f64(0); w.f64(0);
        int16_t payload[6] = {100, 200, 300, 400, 500, 600};
        w.raw(reinterpret_cast<const uint8_t*>(payload), sizeof(payload));
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        PointCloudStreamReader r(src);
        ChannelDescriptor c;
        check(PointBlockParser::ParseChannelDescriptor(r, c), "TEST5 parse channel descriptor");
        auto ch = CreateChannel(c.id, c.format, c.count, c.Data(), c.scale, c.offset, c.stride);
        double out[3] = {0, 0, 0};
        check(ch.ReadXYZ(0, out), "TEST5 decode ok");
        check(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0, "TEST5 int16 scale/offset");
    }

    // TEST 6: Channel payload attaches correctly
    {
        BinWriter w;
        w.u32(0);                 // normal node
        w.f64(0); w.f64(0); w.f64(0); w.f64(5); w.f64(5); w.f64(5);
        w.u32(0);
        w.u32(1);                 // one channel
        w.u32(0); w.u32(1); w.u32(2); w.u32(6);
        w.f64(0.01); w.f64(0.01); w.f64(0.01);
        w.f64(0); w.f64(0); w.f64(0);
        int16_t payload[6] = {100, 200, 300, 10, 20, 30};
        w.raw(reinterpret_cast<const uint8_t*>(payload), sizeof(payload));
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        PointCloudStreamReader r(src);
        bool ok = false;
        auto node = PointBlockParser::ParseNode(r, ok);
        check(ok && node != nullptr, "TEST6 parse node ok");
        const PointAttributeChannel* ch = node->channels().GetChannel(ChannelId::XYZ);
        check(ch != nullptr, "TEST6 channel attached");
        double out[3] = {0, 0, 0};
        check(node->channels().ReadXYZ(0, out), "TEST6 read via node storage");
        check(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0, "TEST6 payload decoded");
    }

    // TEST 7: Multiple nodes attach to cloud
    {
        BinWriter w;
        w.u32(2);                 // blockCount
        w.u32(0); w.u32(1);       // node index table
        w.u32(0); w.f64(0); w.f64(0); w.f64(0); w.f64(1); w.f64(1); w.f64(1); w.u32(0); w.u32(0);
        w.u32(1); w.f64(0); w.f64(0); w.f64(0); w.f64(1); w.f64(1); w.f64(1); w.u32(0); w.u32(0);
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        CloudBlockLoader loader(src);
        PointCloud cloud;
        check(loader.Load(cloud), "TEST7 load cloud");
        auto* root = dynamic_cast<VoxelNode*>(cloud.Root());
        check(root != nullptr, "TEST7 root is voxel");
        check(root->ChildCount() == 2, "TEST7 two nodes attached");
    }

    // TEST 8: FinalizeCloud calculates point count
    {
        BinWriter w;
        w.u32(1);                 // blockCount
        w.u32(0);                 // index table
        w.u32(0); w.f64(0); w.f64(0); w.f64(0); w.f64(1); w.f64(1); w.f64(1); w.u32(0);
        w.u32(1);                 // one channel
        std::vector<float> vals(50 * 3, 0.0f);
        AppendXYZFloat(w, 50, vals);
        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        CloudBlockLoader loader(src);
        PointCloud cloud;
        check(loader.Load(cloud), "TEST8 load cloud");
        check(cloud.PointCount() == 50, "TEST8 point count aggregated");
    }

    // TEST 9: complete pipeline (stream -> parse -> node -> channel -> finalize)
    {
        BinWriter w;
        w.u32(2);                 // blockCount
        w.u32(0); w.u32(1);       // index table
        // block 0: voxel with XYZ(int16) + RGB
        w.u32(1); w.f64(0); w.f64(0); w.f64(0); w.f64(5); w.f64(5); w.f64(5); w.u32(0);
        w.u32(2);                 // two channels
        w.u32(0); w.u32(1); w.u32(10); w.u32(6);                 // XYZ int16
        w.f64(0.01); w.f64(0.01); w.f64(0.01); w.f64(0); w.f64(0); w.f64(0);
        int16_t xyz[30];
        for (int i = 0; i < 30; ++i) xyz[i] = static_cast<int16_t>(100 + i);
        w.raw(reinterpret_cast<const uint8_t*>(xyz), sizeof(xyz));
        w.u32(1); w.u32(2); w.u32(10); w.u32(3);                 // RGB uint8
        w.f64(1); w.f64(1); w.f64(1); w.f64(0); w.f64(0); w.f64(0);
        uint8_t rgb[30];
        for (int i = 0; i < 30; ++i) rgb[i] = static_cast<uint8_t>(i);
        w.raw(rgb, sizeof(rgb));
        // block 1: normal with XYZ float
        w.u32(0); w.f64(0); w.f64(0); w.f64(0); w.f64(5); w.f64(5); w.f64(5); w.u32(0);
        w.u32(1);
        std::vector<float> vals(20 * 3, 1.0f);
        AppendXYZFloat(w, 20, vals);

        MemoryBlockSource src(w.b.data(), w.b.size(), 64);
        CloudBlockLoader loader(src);
        PointCloud cloud;
        check(loader.Load(cloud), "TEST9 load pipeline");

        auto* root = dynamic_cast<VoxelNode*>(cloud.Root());
        check(root && root->ChildCount() == 2, "TEST9 two nodes attached");
        check(cloud.PointCount() == 30, "TEST9 point count = 10 + 20");
        check(cloud.Attributes().Has(PointAttribute::XYZ), "TEST9 XYZ available");
        check(cloud.Attributes().Has(PointAttribute::RGB), "TEST9 RGB available");
        check(root->Child(0)->owner() == &cloud, "TEST9 child owner set");
        // decode a point end-to-end from child 0 (voxel)
        double out[3] = {0, 0, 0};
        check(root->Child(0)->channels().ReadXYZ(0, out), "TEST9 decode via populated cloud");
        check(out[0] == 1.0 && out[1] == 1.01 && out[2] == 1.02, "TEST9 decoded value correct");
    }

    printf("\n%s\n", g_fail == 0 ? "PIECE3_OK" : "PIECE3_FAIL");
    return g_fail == 0 ? 0 : 1;
}
