#pragma once
#include <vector>
#include <map>
#include <array>
#include <cstdint>
#include <mutex>
#include <functional>
#include <string>

namespace workstation {
namespace cad {

struct ShadingConfig {
    float azimuth = 45.0f;
    float elevation = 45.0f;
    float ambient = 0.25f;
    float maxEdgeLength = 10.0f;
    int qualityLevel = 1;
};

struct ShadingMesh {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> shadeValues;
    bool valid = false;
};

class ShadingDisplay {
public:
    ShadingDisplay() = default;
    ~ShadingDisplay() = default;

    void setConfig(const ShadingConfig& config) { m_config = config; }
    ShadingConfig config() const { return m_config; }

    ShadingMesh computeShading(
        const std::vector<float>& xyz,
        const std::vector<uint8_t>& classifications,
        const std::vector<uint8_t>& visibleClasses,
        float gridPrecision = 0.5f
    );

    void invalidateCache();

    std::vector<float> computeHillshade(
        const std::vector<float>& vertexNormals,
        float azimuth,
        float elevation,
        float ambient
    );

    static void computeFaceNormal(const float* v0, const float* v1, const float* v2, float& nx, float& ny, float& nz);

    static std::vector<float> computeVertexNormals(
        const std::vector<float>& vertices,
        const std::vector<uint32_t>& indices
    );

    static std::vector<uint32_t> delaunayTriangulate2D(
        const std::vector<float>& xyPoints
    );

    static std::vector<uint32_t> gridDeduplicate(
        const std::vector<float>& xyPoints,
        float precision
    );

    std::vector<uint32_t> selectRepresentativePoints(
        const std::vector<float>& xyz,
        const std::vector<uint8_t>& classifications,
        const std::vector<uint8_t>& visibleClasses,
        float precision
    );

    using ProgressCallback = std::function<void(int percent, const std::string& message)>;
    void setProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

private:
    float computeEffectiveElevation(float sharpnessOverdrive = 0.0f) const;

    ShadingConfig m_config;
    ProgressCallback m_progressCallback;

    struct CacheEntry {
        ShadingMesh mesh;
        uint64_t dataHash = 0;
        std::vector<uint8_t> visibleClasses;
        float maxEdgeLength = 0;
    };

    static constexpr int kMaxCacheSize = 4;
    std::vector<CacheEntry> m_cache;
    std::mutex m_cacheMutex;
};

} // namespace cad
} // namespace workstation
