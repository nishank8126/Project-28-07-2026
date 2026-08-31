#pragma once
#include "workstation/spatial/SpatialTree.h"
#include "workstation/query/PointCloudQueryManager.h"

namespace workstation { namespace core {

// Clean-room equivalent of the PtVortex / PointCloudCore engine entry point.
// Meyers singleton: first request creates the global instance, further
// requests return the same instance (confirmed PtVortex singleton model).
// The engine owns the spatial index and the query manager.
class PointCloudEngine {
public:
    static PointCloudEngine& instance();

    spatial::SpatialTree& tree() { return tree_; }
    query::PointCloudQueryManager& queries() { return queries_; }

private:
    PointCloudEngine() = default;
    ~PointCloudEngine() = default;
    PointCloudEngine(const PointCloudEngine&) = delete;
    PointCloudEngine& operator=(const PointCloudEngine&) = delete;

    spatial::SpatialTree tree_;
    query::PointCloudQueryManager queries_;
};

} // namespace core
} // namespace workstation
