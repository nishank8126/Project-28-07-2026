#pragma once
#include "workstation/math/Point3d.h"

#include <cstdint>
#include <vector>

namespace workstation {
namespace tools {

struct SectionLine {
    math::Point3d start;
    math::Point3d end;
    double width = 2.0;
};

struct SectionPoint {
    math::Point3d position;
    double station = 0.0;
    double elevation = 0.0;
    uint64_t nodeId = 0;
};

struct SectionResult {
    std::vector<SectionPoint> points;
    double length = 0.0;
    double minWidth = 0.0;
    double maxWidth = 0.0;
    double minHeight = 0.0;
    double maxHeight = 0.0;
};

class SectionTool {
public:
    SectionTool() = default;
    ~SectionTool() = default;

    void SetSectionLine(const math::Point3d& start, const math::Point3d& end);
    void SetWidth(double width) { width_ = width; }
    double GetWidth() const { return width_; }

    const SectionLine& GetSectionLine() const { return line_; }
    bool HasSection() const { return hasSection_; }

    void AddPoint(const math::Point3d& point, uint64_t nodeId = 0);
    void ClearPoints();
    void ComputeResult();

    const SectionResult& GetResult() const { return result_; }

    void RenderUI();

private:
    SectionLine line_;
    std::vector<SectionPoint> points_;
    SectionResult result_;
    bool hasSection_ = false;
    double width_ = 2.0;

    double ProjectPointToStation(const math::Point3d& p) const;
    double ProjectPointToElevation(const math::Point3d& p) const;
    bool IsPointWithinWidth(const math::Point3d& p) const;
};

} // namespace tools
} // namespace workstation
