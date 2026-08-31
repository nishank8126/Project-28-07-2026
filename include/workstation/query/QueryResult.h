#pragma once
#include <cstdint>
#include <vector>

namespace workstation { namespace query {

// Result item stored for an accepted spatial node. Point coordinates are left
// at zero until the real point-retrieval stage (ReadPoints) is reversed.
struct QueryResultItem {
    uint64_t nodeKey = 0;
    uint64_t pointCount = 0;
    double x = 0.0, y = 0.0, z = 0.0;
};

// Result container for a frustum query. Initial capacity is 1024 elements,
// matching the confirmed creation behaviour. push_back performs no separate
// allocation beyond the vector's own growth.
class QueryResult {
    std::vector<QueryResultItem> items_;

public:
    QueryResult() { items_.reserve(1024); }

    void Reserve(size_t n) { if (n > items_.capacity()) items_.reserve(n); }
    void Add(const QueryResultItem& it) { items_.push_back(it); }
    void Clear() { items_.clear(); }

    size_t Count() const { return items_.size(); }
    size_t Capacity() const { return items_.capacity(); }
    const QueryResultItem* Data() const { return items_.data(); }
    const std::vector<QueryResultItem>& Items() const { return items_; }
};

} // namespace query
} // namespace workstation
