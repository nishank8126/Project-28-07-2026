#pragma once
#include "workstation/query/QueryBase.h"
#include "workstation/query/QueryResult.h"
#include "workstation/query/QueryContext.h"
#include "workstation/spatial/SpatialTree.h"
#include "workstation/spatial/SpatialNode.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace workstation { namespace query {

// Clean-room equivalent of Bentley FrustumQuery / ptCreateFrustumPointsQuery.
// Independent name: PointCloudFrustumQuery. No proprietary offsets are used.
class PointCloudFrustumQuery : public QueryBase {
public:
    static PointCloudFrustumQuery* Create();   // ptCreateFrustumPointsQuery
    ~PointCloudFrustumQuery() override;

    uint64_t Execute() override;               // FUN_1800389d0

    // Visibility interface (FUN_180058e40). Exact plane maths pending RE.
    bool TestVisibility(spatial::SpatialNode& node);

    // Viewport point budget (ptPtsToLoadInViewport). NOT a visibility calc.
    uint64_t CalculateViewportBudget(uint64_t identifier);

    float density() const { return density_; }
    uint32_t mode() const { return mode_; }
    bool active() const { return active_; }
    void setActive(bool a) { active_ = a; }

    const std::vector<QueryResultItem>& Results() const { return result_.Items(); }
    size_t ResultCount() const { return result_.Count(); }
    const QueryContext& context() const { return context_; }

    void setFrustumParams(double a, double b, double c, double d) {
        fp_[0] = a; fp_[1] = b; fp_[2] = c; fp_[3] = d;
    }
    void setProgress(void (*cb)(uint64_t, void*), void* user) {
        context_.progress = cb;
        context_.progressUser = user;
    }
    void BindTree(spatial::SpatialTree* t) { tree_ = t; }

private:
    PointCloudFrustumQuery();

    float density_ = 1.0f;                 // density = 1.0
    uint32_t mode_ = 2;                    // mode = 2
    bool active_ = false;                  // active state = false
    double fp_[4] = {0, 0, 0, 0};          // frustum params (encoding pending RE)
    uint64_t timestamp_ = 0;               // first-execution timestamp
    void* callback_ = nullptr;             // query callback
    QueryResult result_;                   // result storage, capacity 1024
    QueryContext context_;
    spatial::SpatialTree* tree_ = nullptr;
    static std::atomic<uint64_t> s_counter_;

    uint64_t convertHandle(uint64_t identifier) const { return identifier; } // pending RE
};

} // namespace query
} // namespace workstation
