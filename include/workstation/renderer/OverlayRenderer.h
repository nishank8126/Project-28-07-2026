#pragma once
#include "workstation/spatial/BoundingBox.h"
#include <cstdint>
#include <vector>

namespace workstation {

namespace renderer {

struct OverlayVertex {
    float position[3];
    float color[4];
};

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer() = default;

    void Initialize() { initialized_ = true; }
    void Shutdown() { Clear(); initialized_ = false; }
    bool IsInitialized() const { return initialized_; }

    void DrawLine(float x0, float y0, float z0,
                  float x1, float y1, float z1,
                  float r, float g, float b, float a = 1.0f, float width = 1.0f);

    void DrawRectangle(float x0, float y0, float x1, float y1,
                       float r, float g, float b, float a = 1.0f, float width = 1.0f);

    void DrawCircle(float cx, float cy, float cz, float radius,
                    float r, float g, float b, float a = 1.0f, float width = 1.0f,
                    int segments = 64);

    void DrawCrosshair(float cx, float cy, float cz, float size,
                       float r, float g, float b, float a = 1.0f);

    void DrawSelectionRectangle(int x0, int y0, int x1, int y1,
                                 int viewportW, int viewportH,
                                 float r, float g, float b, float a = 0.3f);

    void DrawPolygon(const std::vector<std::pair<float,float>>& points,
                     float r, float g, float b, float a = 1.0f, float width = 1.0f);

    void DrawHighlightLine(float x0, float y0, float z0,
                           float x1, float y1, float z1,
                           float width = 3.0f);

    void DrawHighlightPoint(float x, float y, float z, float size = 5.0f);

    const std::vector<OverlayVertex>& GetVertices() const { return vertices_; }
    void Clear() { vertices_.clear(); }
    bool HasGeometry() const { return !vertices_.empty(); }

private:
    std::vector<OverlayVertex> vertices_;
    bool initialized_ = false;
};

} // namespace renderer
} // namespace workstation
