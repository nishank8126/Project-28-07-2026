#include "workstation/cad/ShadingDisplay.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <set>
#include <unordered_map>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace workstation {
namespace cad {

static constexpr double kPi = 3.14159265358979323846;
static constexpr float kDeg2Rad = static_cast<float>(kPi / 180.0);

void ShadingDisplay::invalidateCache() {
    std::lock_guard lock(m_cacheMutex);
    m_cache.clear();
}

void ShadingDisplay::computeFaceNormal(const float* v0, const float* v1, const float* v2, float& nx, float& ny, float& nz) {
    float e1x = v1[0] - v0[0], e1y = v1[1] - v0[1], e1z = v1[2] - v0[2];
    float e2x = v2[0] - v0[0], e2y = v2[1] - v0[1], e2z = v2[2] - v0[2];
    nx = e1y * e2z - e1z * e2y;
    ny = e1z * e2x - e1x * e2z;
    nz = e1x * e2y - e1y * e2x;
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-10f) { nx /= len; ny /= len; nz /= len; }
}

std::vector<float> ShadingDisplay::computeVertexNormals(const std::vector<float>& vertices, const std::vector<uint32_t>& indices) {
    int numVerts = static_cast<int>(vertices.size() / 3);
    int numFaces = static_cast<int>(indices.size() / 3);
    std::vector<float> normals(vertices.size(), 0.0f);

#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 64)
#endif
    for (int f = 0; f < numFaces; ++f) {
        uint32_t i0 = indices[f * 3 + 0], i1 = indices[f * 3 + 1], i2 = indices[f * 3 + 2];
        if (i0 >= numVerts || i1 >= numVerts || i2 >= numVerts) continue;
        float nx, ny, nz;
        computeFaceNormal(&vertices[i0 * 3], &vertices[i1 * 3], &vertices[i2 * 3], nx, ny, nz);
        for (uint32_t idx : {i0, i1, i2}) {
#ifdef _OPENMP
#pragma omp atomic
#endif
            normals[idx * 3 + 0] += nx;
#ifdef _OPENMP
#pragma omp atomic
#endif
            normals[idx * 3 + 1] += ny;
#ifdef _OPENMP
#pragma omp atomic
#endif
            normals[idx * 3 + 2] += nz;
        }
    }

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int i = 0; i < numVerts; ++i) {
        float nx = normals[i * 3 + 0], ny = normals[i * 3 + 1], nz = normals[i * 3 + 2];
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-10f) { normals[i * 3 + 0] = nx / len; normals[i * 3 + 1] = ny / len; normals[i * 3 + 2] = nz / len; }
    }
    return normals;
}

std::vector<float> ShadingDisplay::computeHillshade(const std::vector<float>& vertexNormals, float azimuth, float elevation, float ambient) {
    int numVerts = static_cast<int>(vertexNormals.size() / 3);
    std::vector<float> shade(numVerts);

    float azRad = (360.0f - azimuth + 90.0f) * kDeg2Rad;
    float elRad = elevation * kDeg2Rad;
    float lx = std::cos(elRad) * std::cos(azRad);
    float ly = std::cos(elRad) * std::sin(azRad);
    float lz = std::sin(elRad);
    float lLen = std::sqrt(lx * lx + ly * ly + lz * lz);
    if (lLen > 1e-10f) { lx /= lLen; ly /= lLen; lz /= lLen; }

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int i = 0; i < numVerts; ++i) {
        float nx = vertexNormals[i * 3 + 0], ny = vertexNormals[i * 3 + 1], nz = vertexNormals[i * 3 + 2];
        float dot = nx * lx + ny * ly + nz * lz;
        shade[i] = std::min(1.0f, std::max(0.0f, ambient + (1.0f - ambient) * std::max(0.0f, dot)));
    }
    return shade;
}

std::vector<uint32_t> ShadingDisplay::gridDeduplicate(const std::vector<float>& xyPoints, float precision) {
    int numPoints = static_cast<int>(xyPoints.size() / 2);
    std::map<std::pair<int, int>, uint32_t> grid;
    std::vector<uint32_t> result;
    result.reserve(numPoints);
    float invPrec = 1.0f / precision;

    for (int i = 0; i < numPoints; ++i) {
        int gx = static_cast<int>(std::floor(xyPoints[i * 2 + 0] * invPrec));
        int gy = static_cast<int>(std::floor(xyPoints[i * 2 + 1] * invPrec));
        auto key = std::make_pair(gx, gy);
        if (grid.find(key) == grid.end()) {
            grid[key] = static_cast<uint32_t>(i);
            result.push_back(static_cast<uint32_t>(i));
        }
    }
    return result;
}

std::vector<uint32_t> ShadingDisplay::delaunayTriangulate2D(const std::vector<float>& xyPoints) {
    int numPoints = static_cast<int>(xyPoints.size() / 2);
    std::vector<uint32_t> indices;
    if (numPoints < 3) return indices;

    std::vector<std::array<float, 2>> pts(numPoints);
    for (int i = 0; i < numPoints; ++i) pts[i] = {xyPoints[i * 2], xyPoints[i * 2 + 1]};

    auto circumcircle = [&](uint32_t i0, uint32_t i1, uint32_t i2) -> std::array<float, 3> {
        float ax = pts[i0][0], ay = pts[i0][1], bx = pts[i1][0], by = pts[i1][1], cx = pts[i2][0], cy = pts[i2][1];
        float d = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs(d) < 1e-10f) return {0, 0, 1e10f};
        float ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/d;
        float uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/d;
        float r2 = (ax-ux)*(ax-ux)+(ay-uy)*(ay-uy);
        return {ux, uy, r2};
    };

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t, std::array<float, 3>>> triangles;

    for (int i = 0; i < numPoints - 2 && triangles.size() < 2 * numPoints; ++i) {
        for (int j = i + 1; j < numPoints && triangles.size() < 2 * numPoints; ++j) {
            for (int k = j + 1; k < numPoints && triangles.size() < 2 * numPoints; ++k) {
                auto cc = circumcircle(i, j, k);
                bool valid = true;
                for (int m = 0; m < numPoints && valid; ++m) {
                    if (m == i || m == j || m == k) continue;
                    float dx = pts[m][0] - cc[0], dy = pts[m][1] - cc[1];
                    if (dx*dx + dy*dy < cc[2] - 1e-6f) valid = false;
                }
                if (valid) triangles.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(j), static_cast<uint32_t>(k), cc});
            }
        }
    }

    for (const auto& [i0, i1, i2, cc] : triangles) {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }
    return indices;
}

std::vector<uint32_t> ShadingDisplay::selectRepresentativePoints(
    const std::vector<float>& xyz,
    const std::vector<uint8_t>& classifications,
    const std::vector<uint8_t>& visibleClasses,
    float precision)
{
    int numPoints = static_cast<int>(xyz.size() / 3);
    std::vector<float> xyPoints;
    xyPoints.reserve(numPoints * 2);

    std::set<uint8_t> visibleSet(visibleClasses.begin(), visibleClasses.end());

    for (int i = 0; i < numPoints; ++i) {
        uint8_t cls = (i < static_cast<int>(classifications.size())) ? classifications[i] : 0;
        if (visibleSet.count(cls)) {
            xyPoints.push_back(xyz[i * 3 + 0]);
            xyPoints.push_back(xyz[i * 3 + 1]);
        }
    }
    return gridDeduplicate(xyPoints, precision);
}

float ShadingDisplay::computeEffectiveElevation(float sharpnessOverdrive) const {
    return std::min(89.0f, m_config.elevation + sharpnessOverdrive * 10.0f);
}

ShadingMesh ShadingDisplay::computeShading(
    const std::vector<float>& xyz,
    const std::vector<uint8_t>& classifications,
    const std::vector<uint8_t>& visibleClasses,
    float gridPrecision)
{
    if (m_progressCallback) m_progressCallback(0, "Selecting representative points...");

    auto repIndices = selectRepresentativePoints(xyz, classifications, visibleClasses, gridPrecision);
    if (repIndices.size() < 3) {
        if (m_progressCallback) m_progressCallback(100, "Not enough points for triangulation");
        return {};
    }

    std::vector<float> repXY;
    repXY.reserve(repIndices.size() * 2);
    for (uint32_t idx : repIndices) {
        repXY.push_back(xyz[idx * 3 + 0]);
        repXY.push_back(xyz[idx * 3 + 1]);
    }

    if (m_progressCallback) m_progressCallback(20, "Triangulating...");
    auto triIndices = delaunayTriangulate2D(repXY);
    if (triIndices.size() < 3) {
        if (m_progressCallback) m_progressCallback(100, "Triangulation failed");
        return {};
    }

    std::vector<float> vertices;
    vertices.reserve(repIndices.size() * 3);
    for (uint32_t idx : repIndices) {
        vertices.push_back(xyz[idx * 3 + 0]);
        vertices.push_back(xyz[idx * 3 + 1]);
        vertices.push_back(xyz[idx * 3 + 2]);
    }

    if (m_progressCallback) m_progressCallback(40, "Filtering edges...");
    float maxEdge = m_config.maxEdgeLength * (m_config.qualityLevel == 0 ? 2.0f : m_config.qualityLevel == 2 ? 0.5f : 1.0f);
    float maxEdgeSq = maxEdge * maxEdge;
    int numVerts = static_cast<int>(vertices.size() / 3);
    int numFaces = static_cast<int>(triIndices.size() / 3);

    std::vector<uint32_t> filteredIndices;
    for (int f = 0; f < numFaces; ++f) {
        uint32_t i0 = triIndices[f*3+0], i1 = triIndices[f*3+1], i2 = triIndices[f*3+2];
        if (i0 >= numVerts || i1 >= numVerts || i2 >= numVerts) continue;
        auto edgeSq = [&](uint32_t a, uint32_t b) -> float {
            float dx = vertices[a*3+0]-vertices[b*3+0], dy = vertices[a*3+1]-vertices[b*3+1], dz = vertices[a*3+2]-vertices[b*3+2];
            return dx*dx+dy*dy+dz*dz;
        };
        if (edgeSq(i0,i1) <= maxEdgeSq && edgeSq(i1,i2) <= maxEdgeSq && edgeSq(i2,i0) <= maxEdgeSq) {
            filteredIndices.insert(filteredIndices.end(), {i0, i1, i2});
        }
    }

    if (m_progressCallback) m_progressCallback(60, "Computing normals...");
    auto normals = computeVertexNormals(vertices, filteredIndices);

    if (m_progressCallback) m_progressCallback(80, "Computing hillshade...");
    float effElevation = computeEffectiveElevation();
    auto shadeValues = computeHillshade(normals, m_config.azimuth, effElevation, m_config.ambient);

    std::vector<float> colors;
    colors.reserve(shadeValues.size() * 3);
    for (float s : shadeValues) {
        colors.push_back(s);
        colors.push_back(s);
        colors.push_back(s);
    }

    ShadingMesh mesh;
    mesh.vertices = vertices;
    mesh.indices = filteredIndices;
    mesh.normals = normals;
    mesh.colors = colors;
    mesh.shadeValues = shadeValues;
    mesh.valid = true;

    if (m_progressCallback) m_progressCallback(100, "Shading complete");
    return mesh;
}

} // namespace cad
} // namespace workstation
