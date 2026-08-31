#include "workstation/pointcloud/PointStorage.h"

namespace workstation { namespace pointcloud {

// All PointStorage behaviour is inline in the header (it only delegates to the
// owned PointAttributeChannel objects). This translation unit is kept so the
// class has an independent compilation unit for future non-inline growth.

} // namespace pointcloud
} // namespace workstation
