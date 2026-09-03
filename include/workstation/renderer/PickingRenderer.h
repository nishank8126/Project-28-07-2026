#pragma once
#include "workstation/math/Point3d.h"
#include <cstdint>

namespace workstation {
namespace renderer {

class PickingRenderer {
public:
    PickingRenderer() = default;
    ~PickingRenderer() = default;

    void Initialize() { initialized_ = true; }
    void Shutdown() { initialized_ = false; }
    bool IsInitialized() const { return initialized_; }

    struct PickingPixel {
        uint32_t objectID = 0;
        uint32_t entityID = 0;
        uint32_t primitiveID = 0;
    };

    PickingPixel ReadPixel(int x, int y) const {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return {};
        int idx = y * width_ + x;
        if (idx < 0 || idx >= static_cast<int>(pixels_.size())) return {};
        return pixels_[idx];
    }

    void SetSize(int width, int height) {
        width_ = width;
        height_ = height;
        pixels_.resize(width * height);
    }

    void ClearPixels() {
        std::fill(pixels_.begin(), pixels_.end(), PickingPixel{});
    }

    void SetPixel(int x, int y, uint32_t objectID, uint32_t entityID, uint32_t primitiveID) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        pixels_[y * width_ + x] = {objectID, entityID, primitiveID};
    }

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool e) { enabled_ = e; }

private:
    std::vector<PickingPixel> pixels_;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    bool enabled_ = false;
};

} // namespace renderer
} // namespace workstation
