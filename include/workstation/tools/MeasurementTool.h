#pragma once
#include "workstation/math/Point3d.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace workstation {
namespace tools {

enum class MeasurementMode {
    Distance,
    Area,
    Height,
    Volume,
    None
};

struct MeasurementPoint {
    math::Point3d position;
    uint64_t nodeId = 0;
};

struct MeasurementResult {
    MeasurementMode mode = MeasurementMode::None;
    double distance = 0.0;
    double area = 0.0;
    double heightDiff = 0.0;
    double volume = 0.0;
    std::vector<MeasurementPoint> points;
    std::string label;
};

class MeasurementTool {
public:
    MeasurementTool() = default;
    ~MeasurementTool() = default;

    void SetMode(MeasurementMode mode) { mode_ = mode; }
    MeasurementMode GetMode() const { return mode_; }

    void AddPoint(const math::Point3d& point, uint64_t nodeId = 0);
    void UndoLastPoint();
    void ClearPoints();

    const std::vector<MeasurementPoint>& GetPoints() const { return points_; }
    const MeasurementResult& GetResult() const { return result_; }

    void SetLabel(const std::string& label) { result_.label = label; }

    void RenderUI();

private:
    MeasurementMode mode_ = MeasurementMode::None;
    std::vector<MeasurementPoint> points_;
    MeasurementResult result_;

    void ComputeResult();
    double ComputeDistance(const math::Point3d& a, const math::Point3d& b) const;
    double ComputeTriangleArea(const math::Point3d& a, const math::Point3d& b, const math::Point3d& c) const;
};

} // namespace tools
} // namespace workstation
