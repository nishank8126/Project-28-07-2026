#include "workstation/core/PointCloudScene.h"
#include "workstation/pointcloud/PointCloud.h"
#include "workstation/pointcloud/PointStorage.h"
#include "workstation/pointcloud/PointAttributeChannel.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/pointcloud/PointAttributeMask.h"
#include "workstation/pointcloud/BoundingBox.h"

#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>

static int g_fail = 0;
static void check(bool c, const char* m) {
    if (!c) { printf("  FAIL: %s\n", m); ++g_fail; }
    else    { printf("  PASS: %s\n", m); }
}

int main() {
    using namespace workstation;
    printf("== NakshaPointEngine :: Piece 2 (Scene/Cloud/Node/Voxel/Channel/Storage) ==\n\n");

    pointcloud::BoundingBox bb;
    bb.minX=0; bb.minY=0; bb.minZ=0; bb.maxX=10; bb.maxY=10; bb.maxZ=10;

    // TEST 1: scene with multiple clouds
    {
        auto& scene = core::PointCloudScene::instance();
        auto* a = scene.CreateCloud("alpha");
        auto* b = scene.CreateCloud("beta");
        check(a != nullptr && b != nullptr, "TEST1 two clouds created");
        check(scene.CloudCount() >= 2, "TEST1 scene holds multiple clouds");
    }

    // TEST 2: PointCloudNode with bounding box
    {
        auto n = pointcloud::CreatePointNode(bb);
        check(n != nullptr, "TEST2 node created");
        check(n->bounds().maxX == 10.0, "TEST2 bounds stored");
        check(!n->IsVoxel(), "TEST2 node is not a voxel");
    }

    // TEST 3: VoxelNode density=1.0 and children container exists
    {
        auto v = pointcloud::CreateVoxelNode(bb);
        check(v->IsVoxel(), "TEST3 IsVoxel true");
        check(v->density() == 1.0, "TEST3 density = 1.0");
        check(v->ChildCount() == 0, "TEST3 children container exists (empty)");
        v->AddChild(pointcloud::CreatePointNode(bb));
        check(v->ChildCount() == 1, "TEST3 child can be added");
    }

    // TEST 4: channel element size mapping (FUN_18006e2f0)
    {
        struct { pointcloud::PointChannelType t; size_t sz; } tbl[] = {
            {pointcloud::PointChannelType::Type1, 4},
            {pointcloud::PointChannelType::Type2, 8},
            {pointcloud::PointChannelType::Type4, 1},
            {pointcloud::PointChannelType::Type5, 1},
            {pointcloud::PointChannelType::Type6, 2},
            {pointcloud::PointChannelType::Type7, 2},
            {pointcloud::PointChannelType::Type8, 4},
            {pointcloud::PointChannelType::Type9, 4},
            {pointcloud::PointChannelType::Type10, 8},
        };
        bool ok = true;
        for (auto& e : tbl)
            if (pointcloud::GetElementSize(e.t) != e.sz) ok = false;
        check(ok, "TEST4 GetElementSize matches RE table");
    }

    // TEST 5: XYZ int16 storage + decode
    {
        int16_t buf[3] = {100, 200, 300};
        double s[3] = {0.01, 0.01, 0.01};
        double o[3] = {0, 0, 0};
        auto ch = pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                            pointcloud::PointFormat::Int16, 1, buf, s, o, 0);
        double out[3] = {0, 0, 0};
        check(ch.ReadXYZ(0, out), "TEST5 int16 decode ok");
        check(out[0] == 1.0 && out[1] == 2.0 && out[2] == 3.0, "TEST5 int16 world coords");
    }

    // TEST 6: XYZ float storage + decode
    {
        float buf[3] = {1.5f, 2.5f, 3.5f};
        auto ch = pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                            pointcloud::PointFormat::Float32, 1, buf, nullptr, nullptr, 0);
        double out[3] = {0, 0, 0};
        check(ch.ReadXYZ(0, out), "TEST6 float decode ok");
        check(out[0] == 1.5 && out[1] == 2.5 && out[2] == 3.5, "TEST6 float world coords");
    }

    // TEST 7: scale + offset transformation
    {
        int16_t v[3] = {100, 100, 100};
        double s[3] = {2, 2, 2};
        double o[3] = {5, 5, 5};
        auto ch = pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                            pointcloud::PointFormat::Int16, 1, v, s, o, 0);
        double out[3] = {0, 0, 0};
        check(ch.ReadXYZ(0, out), "TEST7 transform decode ok");
        check(out[0] == 205.0 && out[1] == 205.0 && out[2] == 205.0, "TEST7 scale+offset applied");
    }

    // TEST 8: RGB channel decode
    {
        uint8_t buf[3] = {255, 128, 0};
        auto ch = pointcloud::CreateChannel(pointcloud::ChannelId::RGB,
                                            pointcloud::PointFormat::UInt8, 1, buf, nullptr, nullptr, 0);
        uint8_t out[3] = {0, 0, 0};
        check(ch.ReadRGB(0, out), "TEST8 rgb decode ok");
        check(out[0] == 255 && out[1] == 128 && out[2] == 0, "TEST8 rgb values");
    }

    // TEST 9: Finalize cloud ownership
    {
        auto cloud = std::make_unique<pointcloud::PointCloud>();
        auto root = pointcloud::CreateVoxelNode(bb);
        auto leaf = pointcloud::CreatePointNode(bb);
        std::vector<int16_t> xyzbuf(300, 0);
        pointcloud::PointAttributeChannel xyz =
            pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                     pointcloud::PointFormat::Int16, 100, xyzbuf.data(), nullptr, nullptr, 0);
        leaf->channels().AddChannel(std::move(xyz));
        root->AddChild(std::move(leaf));
        cloud->SetRoot(root.get());
        cloud->Finalize();
        check(cloud->Root()->owner() == cloud.get(), "TEST9 root owner set");
        check(root->Child(0)->owner() == cloud.get(), "TEST9 child owner set");
    }

    // TEST 10: point count aggregation
    {
        auto cloud = std::make_unique<pointcloud::PointCloud>();
        auto root = pointcloud::CreateVoxelNode(bb);
        auto leaf = pointcloud::CreatePointNode(bb);
        std::vector<int16_t> xyzbuf(300, 0);
        pointcloud::PointAttributeChannel xyz =
            pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                     pointcloud::PointFormat::Int16, 100, xyzbuf.data(), nullptr, nullptr, 0);
        leaf->channels().AddChannel(std::move(xyz));
        root->AddChild(std::move(leaf));
        cloud->SetRoot(root.get());
        cloud->Finalize();
        check(cloud->PointCount() == 100, "TEST10 point count aggregated");
    }

    // TEST 11: attribute availability tracking
    {
        auto cloud = std::make_unique<pointcloud::PointCloud>();
        auto root = pointcloud::CreateVoxelNode(bb);
        auto leaf = pointcloud::CreatePointNode(bb);
        std::vector<int16_t> xyzbuf(300, 0);
        uint8_t rgbbuf[3] = {1, 2, 3};
        leaf->channels().AddChannel(
            pointcloud::CreateChannel(pointcloud::ChannelId::XYZ,
                                     pointcloud::PointFormat::Int16, 100, xyzbuf.data(), nullptr, nullptr, 0));
        leaf->channels().AddChannel(
            pointcloud::CreateChannel(pointcloud::ChannelId::RGB,
                                     pointcloud::PointFormat::UInt8, 100, rgbbuf, nullptr, nullptr, 0));
        root->AddChild(std::move(leaf));
        cloud->SetRoot(root.get());
        cloud->Finalize();
        check(cloud->Attributes().Has(pointcloud::PointAttribute::XYZ), "TEST11 XYZ available");
        check(cloud->Attributes().Has(pointcloud::PointAttribute::RGB), "TEST11 RGB available");
    }

    printf("\n%s\n", g_fail == 0 ? "PIECE2_OK" : "PIECE2_FAIL");
    return g_fail == 0 ? 0 : 1;
}
