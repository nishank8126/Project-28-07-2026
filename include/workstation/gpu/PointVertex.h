#pragma once

namespace workstation {
namespace gpu {

struct PointVertex {
    float position[3] = {};
    float color[3] = {};
    float intensity = 0.0f;
    float classification = 0.0f;
    float normal[3] = {};
};

} // namespace gpu
} // namespace workstation
