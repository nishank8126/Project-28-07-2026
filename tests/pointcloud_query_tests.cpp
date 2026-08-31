#include "workstation/core/PointCloudEngine.h"
#include "workstation/query/PointCloudQueryManager.h"
#include "workstation/query/PointCloudFrustumQuery.h"
#include "workstation/query/PointQueryExecutor.h"
#include "workstation/query/QueryResult.h"
#include "workstation/spatial/SpatialTree.h"
#include "workstation/spatial/SpatialNode.h"
#include "workstation/pointcloud/PointChannelManager.h"

#include <cstdio>
#include <cstdint>

static int g_fail = 0;
static void check(bool c, const char* m) {
    if (!c) { printf("  FAIL: %s\n", m); ++g_fail; }
    else    { printf("  PASS: %s\n", m); }
}

// TEST 8: a rejecting executor proves the visibility interface can reject.
class RejectExecutor : public workstation::query::PointQueryExecutor {
public:
    bool TestVisibility(workstation::spatial::SpatialNode&) override { return false; }
};

// TEST (GetState): set the internal state pointer to exercise both branches.
class StateExecutor : public workstation::query::PointQueryExecutor {
public:
    void SetState(void* p) { internalState_ = p; }
};

int main() {
    using namespace workstation;
    printf("== NakshaPointEngine :: Piece 1 (Point Cloud Query Foundation) ==\n\n");

    // TEST 1: query IDs increment correctly
    {
        auto* a = query::PointCloudFrustumQuery::Create();
        auto* b = query::PointCloudFrustumQuery::Create();
        check(a->Id() != 0, "TEST1 ids are non-zero");
        check(b->Id() == a->Id() + 1, "TEST1 query IDs increment by 1");
        delete a; delete b;
    }

    // TEST 2: frustum query initializes with density=1.0, mode=2
    {
        auto* q = query::PointCloudFrustumQuery::Create();
        check(q->density() == 1.0f, "TEST2 density = 1.0");
        check(q->mode() == 2, "TEST2 mode = 2");
        check(q->active() == false, "TEST2 active = false");
        check(q->Name() != nullptr && q->Name()[0] == 'F', "TEST2 name = FRUSTUM");
        delete q;
    }

    // TEST 3: result container initializes with capacity 1024
    {
        query::QueryResult r;
        check(r.Capacity() >= 1024, "TEST3 result capacity >= 1024");
    }

    // TEST 4: spatial tree insertion works
    {
        spatial::SpatialTree t;
        t.Insert(10, 100);
        t.Insert(5, 50);
        t.Insert(15, 150);
        check(t.Size() == 3, "TEST4 three nodes inserted");
        check(t.Find(10) != nullptr, "TEST4 existing key found");
        check(t.Find(99) == nullptr, "TEST4 missing key not found");
    }

    // TEST 5: spatial tree lookup follows key comparison
    {
        spatial::SpatialTree t;
        t.Insert(10, 100);
        t.Insert(5, 55);
        t.Insert(15, 155);
        auto* a = t.Find(5);
        auto* b = t.Find(15);
        check(a && a->pointCount == 55, "TEST5 left branch lookup correct");
        check(b && b->pointCount == 155, "TEST5 right branch lookup correct");
    }

    // TEST 6: point count accumulation returns correct value
    {
        spatial::SpatialTree t;
        t.Insert(10, 100);
        t.Insert(5, 50);
        t.Insert(15, 150);
        check(t.Accumulate(10) == 300, "TEST6 subtree accumulation = 300");
    }

    // TEST 7: point channels support 32 channels
    {
        pointcloud::PointChannelManager cm;
        for (int i = 0; i < 32; ++i)
            cm.Add(pointcloud::PointChannel{static_cast<pointcloud::ChannelId>(i), nullptr, 4, 0});
        check(cm.Count() == 32, "TEST7 32 channels supported");
        check(!cm.Add(pointcloud::PointChannel{pointcloud::ChannelId::XYZ, nullptr, 4, 0}),
              "TEST7 33rd channel rejected");
    }

    // TEST 8: visibility test interface can accept/reject nodes
    {
        spatial::SpatialNode n;
        n.key = 1;

        query::PointQueryExecutor accept;
        check(accept.TestVisibility(n) == true, "TEST8 accept executor returns true");

        RejectExecutor reject;
        check(reject.TestVisibility(n) == false, "TEST8 reject executor returns false");

        // full pipeline with accept-all -> collects every node
        spatial::SpatialTree t;
        t.Insert(1, 10); t.Insert(2, 20); t.Insert(3, 30);
        auto* q = query::PointCloudFrustumQuery::Create();
        q->BindTree(&t);
        check(q->Execute() == 3, "TEST8 accept-all pipeline collects 3 nodes");
        delete q;
    }

    // TEST 9: query execution pipeline runs end to end
    {
        spatial::SpatialTree t;
        t.Insert(1, 10); t.Insert(2, 20); t.Insert(3, 30); t.Insert(4, 40);
        auto* q = query::PointCloudFrustumQuery::Create();
        q->BindTree(&t);
        uint64_t n = q->Execute();
        check(n == 4, "TEST9 pipeline traverses 4 nodes");
        check(q->ResultCount() == 4, "TEST9 results collected");
        check(q->context().processedPoints == 4, "TEST9 processed count updated");
        check(q->active() == true, "TEST9 active set after execute");
        bool populated = true;
        for (const auto& it : q->Results())
            if (it.pointCount == 0) populated = false;
        check(populated, "TEST9 result items populated");
        delete q;
    }

    // Extra: GetState masking (FUN_180045040)
    {
        StateExecutor se;
        int marker = 0;
        se.SetState(&marker);
        uint64_t same = se.GetState(&marker);
        uint64_t diff = se.GetState(nullptr);
        check((same & 1) == 1, "GetState comparison==state -> valid flag set");
        check((diff & 1) == 0, "GetState comparison!=state -> no valid flag");
    }

    printf("\n%s\n", g_fail == 0 ? "PIECE1_OK" : "PIECE1_FAIL");
    return g_fail == 0 ? 0 : 1;
}
