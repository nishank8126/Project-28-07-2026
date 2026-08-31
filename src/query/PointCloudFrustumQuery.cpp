#include "workstation/query/PointCloudFrustumQuery.h"
#include "workstation/query/PointQueryExecutor.h"
#include "workstation/core/PointCloudEngine.h"

#include <atomic>
#include <chrono>

namespace workstation { namespace query {

std::atomic<uint64_t> PointCloudFrustumQuery::s_counter_{1};

PointCloudFrustumQuery::PointCloudFrustumQuery() {
    id_ = s_counter_.fetch_add(1);
    name_ = "FRUSTUM";
}

PointCloudFrustumQuery::~PointCloudFrustumQuery() {
    core::PointCloudEngine::instance().queries().Unregister(id_);
}

PointCloudFrustumQuery* PointCloudFrustumQuery::Create() {
    PointCloudFrustumQuery* q = new PointCloudFrustumQuery();
    q->tree_ = &core::PointCloudEngine::instance().tree();
    core::PointCloudEngine::instance().queries().Register(q);
    return q;
}

uint64_t PointCloudFrustumQuery::Execute() {
    // 1. first execution -> store start timestamp
    if (timestamp_ == 0) {
        timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    active_ = true;

    // 2. prepare query execution context
    context_.timestampMs = timestamp_;

    // 3. create point reader object
    PointQueryExecutor executor;
    executor.setBounds(spatial::BoundingBox{});
    executor.SetResultBuffer(&result_);

    // 4. build traversal state (none beyond the tree visitor)

    // 5. spatial traversal
    result_.Clear();
    if (tree_) {
        tree_->Traverse([&](spatial::SpatialNode& n) {
            // 6. visibility test per candidate
            if (executor.TestVisibility(n)) {
                // 7. store accepted node in result container
                QueryResultItem it;
                it.nodeKey = n.key;
                it.pointCount = n.pointCount;
                result_.Add(it);
            }
        });
    }

    // 8. update counters + progress
    context_.processedPoints = result_.Count();
    context_.lastRequestedPoints = result_.Count();
    context_.lastLoadedPoints = result_.Count();
    if (context_.progress)
        context_.progress(context_.processedPoints, context_.progressUser);

    // 9. return processed result count
    return result_.Count();
}

bool PointCloudFrustumQuery::TestVisibility(spatial::SpatialNode& node) {
    PointQueryExecutor ex;
    return ex.TestVisibility(node);
}

uint64_t PointCloudFrustumQuery::CalculateViewportBudget(uint64_t identifier) {
    // 1. convert external identifier into internal key (pending RE)
    uint64_t key = convertHandle(identifier);
    // 2-5. access tree, traverse, find, accumulate
    uint64_t acc = tree_ ? tree_->Accumulate(key) : 0;
    // 6. store
    context_.lastRequestedPoints = acc;
    context_.lastLoadedPoints = acc;
    // 7. return
    return acc;
}

} // namespace query
} // namespace workstation
