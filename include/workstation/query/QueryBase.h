#pragma once
#include <cstdint>

namespace workstation { namespace query {

// Base type for all query objects. The engine drives queries through Execute().
class QueryBase {
public:
    virtual ~QueryBase() = default;
    virtual uint64_t Execute() = 0;

    uint64_t Id() const { return id_; }
    const char* Name() const { return name_; }

protected:
    uint64_t id_ = 0;
    const char* name_ = "";
};

} // namespace query
} // namespace workstation
