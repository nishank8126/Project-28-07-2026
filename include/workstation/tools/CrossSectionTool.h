#pragma once
#include "workstation/math/Point3d.h"

#include <cstdint>
#include <vector>
#include <string>

namespace workstation {
namespace tools {

enum class CrossSectionType {
    Road,
    Terrain,
    Railway,
    Custom
};

struct CrossSectionPoint {
    math::Point3d position;
    double station = 0.0;
    double offset = 0.0;
    double elevation = 0.0;
    uint32_t classification = 0;
    uint64_t nodeId = 0;
};

struct CrossSectionProfile {
    std::vector<CrossSectionPoint> points;
    double station = 0.0;
    double length = 0.0;
    double leftOffset = 0.0;
    double rightOffset = 0.0;
    double minHeight = 0.0;
    double maxHeight = 0.0;
};

struct CrossSectionResult {
    std::vector<CrossSectionProfile> profiles;
    CrossSectionType type = CrossSectionType::Road;
    uint32_t profileCount = 0;
};

class CrossSectionTool {
public:
    CrossSectionTool() = default;
    ~CrossSectionTool() = default;

    void SetType(CrossSectionType type) { type_ = type; }
    CrossSectionType GetType() const { return type_; }

    void SetAlignment(const math::Point3d& start, const math::Point3d& end);
    void SetProfileSpacing(double spacing) { spacing_ = spacing; }
    void SetProfileWidth(double width) { width_ = width; }

    void AddPoint(const math::Point3d& point, uint64_t nodeId = 0, uint32_t classification = 0);
    void ClearPoints();
    void ComputeProfiles();

    const CrossSectionResult& GetResult() const { return result_; }
    bool HasProfiles() const { return result_.profileCount > 0; }

    void RenderUI();

private:
    CrossSectionType type_ = CrossSectionType::Road;
    math::Point3d alignStart_;
    math::Point3d alignEnd_;
    double spacing_ = 10.0;
    double width_ = 20.0;

    std::vector<CrossSectionPoint> allPoints_;
    CrossSectionResult result_;

    double ComputeStation(const math::Point3d& p) const;
    double ComputeOffset(const math::Point3d& p) const;
    bool IsPointWithinWidth(const math::Point3d& p) const;
};

} // namespace tools
} // namespace workstation
