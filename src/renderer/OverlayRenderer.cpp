#include "workstation/renderer/OverlayRenderer.h"
#include <cmath>

namespace workstation {
namespace renderer {

static constexpr float PI = 3.14159265358979f;

void OverlayRenderer::DrawLine(float x0, float y0, float z0,
                                float x1, float y1, float z1,
                                float r, float g, float b, float a, float width) {
    (void)width;
    vertices_.push_back({x0, y0, z0, r, g, b, a});
    vertices_.push_back({x1, y1, z1, r, g, b, a});
}

void OverlayRenderer::DrawRectangle(float x0, float y0, float x1, float y1,
                                     float r, float g, float b, float a, float width) {
    DrawLine(x0, y0, 0, x1, y0, 0, r, g, b, a, width);
    DrawLine(x1, y0, 0, x1, y1, 0, r, g, b, a, width);
    DrawLine(x1, y1, 0, x0, y1, 0, r, g, b, a, width);
    DrawLine(x0, y1, 0, x0, y0, 0, r, g, b, a, width);
}

void OverlayRenderer::DrawCircle(float cx, float cy, float cz, float radius,
                                  float r, float g, float b, float a, float width,
                                  int segments) {
    (void)width;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float(i) / segments) * 2.0f * PI;
        float a1 = (float(i + 1) / segments) * 2.0f * PI;
        vertices_.push_back({cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, cz, r, g, b, a});
        vertices_.push_back({cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, cz, r, g, b, a});
    }
}

void OverlayRenderer::DrawCrosshair(float cx, float cy, float cz, float size,
                                      float r, float g, float b, float a) {
    DrawLine(cx - size, cy, cz, cx + size, cy, cz, r, g, b, a);
    DrawLine(cx, cy - size, cz, cx, cy + size, cz, r, g, b, a);
}

void OverlayRenderer::DrawSelectionRectangle(int x0, int y0, int x1, int y1,
                                              int viewportW, int viewportH,
                                              float r, float g, float b, float a) {
    (void)viewportW; (void)viewportH;
    float fx0 = float(x0), fy0 = float(y0);
    float fx1 = float(x1), fy1 = float(y1);
    float z = 0.0f;

    vertices_.push_back({fx0, fy0, z, r, g, b, a});
    vertices_.push_back({fx1, fy0, z, r, g, b, a});

    vertices_.push_back({fx1, fy0, z, r, g, b, a});
    vertices_.push_back({fx1, fy1, z, r, g, b, a});

    vertices_.push_back({fx1, fy1, z, r, g, b, a});
    vertices_.push_back({fx0, fy1, z, r, g, b, a});

    vertices_.push_back({fx0, fy1, z, r, g, b, a});
    vertices_.push_back({fx0, fy0, z, r, g, b, a});
}

void OverlayRenderer::DrawPolygon(const std::vector<std::pair<float,float>>& points,
                                   float r, float g, float b, float a, float width) {
    (void)width;
    if (points.size() < 2) return;
    for (size_t i = 0; i < points.size(); ++i) {
        size_t j = (i + 1) % points.size();
        vertices_.push_back({points[i].first, points[i].second, 0, r, g, b, a});
        vertices_.push_back({points[j].first, points[j].second, 0, r, g, b, a});
    }
}

void OverlayRenderer::DrawHighlightLine(float x0, float y0, float z0,
                                          float x1, float y1, float z1,
                                          float width) {
    DrawLine(x0, y0, z0, x1, y1, z1, 1.0f, 1.0f, 0.0f, 1.0f, width);
}

void OverlayRenderer::DrawHighlightPoint(float x, float y, float z, float size) {
    float half = size * 0.5f;
    DrawLine(x - half, y, z, x + half, y, z, 1.0f, 1.0f, 0.0f, 1.0f);
    DrawLine(x, y - half, z, x, y + half, z, 1.0f, 1.0f, 0.0f, 1.0f);
    DrawLine(x, y, z - half, x, y, z + half, 1.0f, 1.0f, 0.0f, 1.0f);
}

} // namespace renderer
} // namespace workstation
