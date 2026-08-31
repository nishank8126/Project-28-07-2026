#pragma once
#include "workstation/spatial/BoundingBox.h"
#include "workstation/spatial/SpatialNode.h"
#include "workstation/query/QueryContext.h"
#include "workstation/pointcloud/PointChannelManager.h"
#include "workstation/query/QueryResult.h"

#include <cstddef>
#include <cstdint>

namespace workstation { namespace query {

// Clean-room equivalent of Bentley querydetail::ReadPoints<Frustum>.
// Responsibilities (confirmed): maintain query state, manage point channels,
// collect point blocks, store query results.
//
// Only confirmed behaviours are implemented:
//  - GetState() reproduces the exact masking behaviour recovered from
//    FUN_180045040 (reads an internal state pointer).
//  - TestVisibility() is a replaceable placeholder: the exact Bentley frustum
//    plane mathematics are NOT yet reversed, so it currently accepts all nodes.
// No proprietary offsets or class names are reproduced in this type.
class PointQueryExecutor {
public:
    virtual ~PointQueryExecutor() = default;                  // vtable[0]
    virtual uint64_t GetState(void* comparison) const;        // vtable[1]
    virtual uint64_t Reserved2() { return 0; }               // vtable[2]
    virtual uint64_t Reserved3() { return 0; }              // vtable[3]
    virtual bool TestVisibility(spatial::SpatialNode& node);  // vtable[4] (pending RE)

    const spatial::BoundingBox& bounds() const { return bounds_; }
    void setBounds(const spatial::BoundingBox& b) { bounds_ = b; }

    QueryContext& context() { return context_; }
    const QueryContext& context() const { return context_; }

    pointcloud::PointChannelManager& channels() { return channels_; }
    const pointcloud::PointChannelManager& channels() const { return channels_; }

    void SetResultBuffer(QueryResult* r) { resultBuffer_ = r; }
    QueryResult* ResultBuffer() { return resultBuffer_; }

protected:
    spatial::BoundingBox bounds_;
    QueryContext context_;
    pointcloud::PointChannelManager channels_;
    QueryResult* resultBuffer_ = nullptr;
    void* internalState_ = nullptr;   // state pointer read by GetState()
};

} // namespace query
} // namespace workstation
