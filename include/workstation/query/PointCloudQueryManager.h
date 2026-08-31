#pragma once
#include "workstation/query/QueryBase.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace workstation { namespace query {

// Confirmed concept: PointCloudQueryManager. Owns registered query objects by
// id and is thread-safe (dynamic API may create queries from multiple threads).
class PointCloudQueryManager {
    mutable std::mutex mtx_;
    std::unordered_map<uint64_t, QueryBase*> map_;

public:
    void Register(QueryBase* q) {
        std::lock_guard<std::mutex> l(mtx_);
        map_[q->Id()] = q;
    }
    void Unregister(uint64_t id) {
        std::lock_guard<std::mutex> l(mtx_);
        map_.erase(id);
    }
    QueryBase* Find(uint64_t id) const {
        std::lock_guard<std::mutex> l(mtx_);
        auto it = map_.find(id);
        return it == map_.end() ? nullptr : it->second;
    }
    size_t Count() const {
        std::lock_guard<std::mutex> l(mtx_);
        return map_.size();
    }
};

} // namespace query
} // namespace workstation
