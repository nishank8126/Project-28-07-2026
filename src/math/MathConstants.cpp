#include "workstation/math/MathConstants.h"

namespace workstation {
namespace math {

const char* MatrixStorageConvention() {
    // OUR explicit choice for the clean-room math library (documented in
    // MathFoundation.md / ADR-007). Not derived from any proprietary layout.
    return "row-major (m[row][column])";
}

} // namespace math
} // namespace workstation
