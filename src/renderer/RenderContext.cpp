#include "workstation/renderer/RenderContext.h"

namespace workstation {
namespace renderer {

void RenderContext::UpdateFrameStats(double frameTimeMs) {
    stats_.frameTimeMs = frameTimeMs;
    stats_.fps = (frameTimeMs > 0.0) ? 1000.0 / frameTimeMs : 0.0;
}

} // namespace renderer
} // namespace workstation
