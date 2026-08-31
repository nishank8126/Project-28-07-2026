#include "workstation/core/PointCloudEngine.h"

namespace workstation { namespace core {

PointCloudEngine& PointCloudEngine::instance() {
    static PointCloudEngine inst;
    return inst;
}

} // namespace core
} // namespace workstation
