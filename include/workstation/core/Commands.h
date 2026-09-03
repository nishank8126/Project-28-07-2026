#pragma once
#include "workstation/core/Command.h"
#include "workstation/math/Point3d.h"
#include <string>
#include <vector>

namespace workstation {
namespace core {

struct CadLineGeometry {
    math::Point3d start = {0, 0, 0};
    math::Point3d end = {0, 0, 0};
    std::string layer;
    double lineWidth = 1.0;
    uint8_t colorR = 255, colorG = 255, colorB = 255;
};

struct CadPolylineGeometry {
    std::vector<math::Point3d> points;
    bool closed = false;
    std::string layer;
};

struct CadCircleGeometry {
    math::Point3d center = {0, 0, 0};
    double radius = 0.0;
    std::string layer;
};

struct CadPolygonGeometry {
    std::vector<math::Point3d> points;
    std::string layer;
};

struct MeasurementGeometry {
    math::Point3d start = {0, 0, 0};
    math::Point3d end = {0, 0, 0};
    double distance = 0.0;
    std::string label;
};

struct ClassificationChange {
    uint64_t pointId = 0;
    uint8_t oldClass = 0;
    uint8_t newClass = 0;
};

class CreateLineCommand : public Command {
public:
    CreateLineCommand(CadLineGeometry line);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Create Line"; }
    CadLineGeometry& GetGeometry() { return line_; }
private:
    CadLineGeometry line_;
    bool executed_ = false;
};

class CreatePolylineCommand : public Command {
public:
    CreatePolylineCommand(CadPolylineGeometry poly);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Create Polyline"; }
    CadPolylineGeometry& GetGeometry() { return poly_; }
private:
    CadPolylineGeometry poly_;
    bool executed_ = false;
};

class CreateCircleCommand : public Command {
public:
    CreateCircleCommand(CadCircleGeometry circle);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Create Circle"; }
    CadCircleGeometry& GetGeometry() { return circle_; }
private:
    CadCircleGeometry circle_;
    bool executed_ = false;
};

class CreatePolygonCommand : public Command {
public:
    CreatePolygonCommand(CadPolygonGeometry polygon);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Create Polygon"; }
    CadPolygonGeometry& GetGeometry() { return polygon_; }
private:
    CadPolygonGeometry polygon_;
    bool executed_ = false;
};

class CreateMeasurementCommand : public Command {
public:
    CreateMeasurementCommand(MeasurementGeometry meas);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Create Measurement"; }
    MeasurementGeometry& GetGeometry() { return meas_; }
    bool IsExecuted() const { return executed_; }
private:
    MeasurementGeometry meas_;
    bool executed_ = false;
};

class ChangeClassificationCommand : public Command {
public:
    ChangeClassificationCommand(std::vector<ClassificationChange> changes);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Change Classification"; }
private:
    std::vector<ClassificationChange> changes_;
};

class MoveEntityCommand : public Command {
public:
    MoveEntityCommand(uint64_t entityId, math::Point3d oldPos, math::Point3d newPos);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Move Entity"; }
private:
    uint64_t entityId_;
    math::Point3d oldPos_;
    math::Point3d newPos_;
};

class DeleteEntityCommand : public Command {
public:
    DeleteEntityCommand(uint64_t entityId);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Delete Entity"; }
private:
    uint64_t entityId_;
};

class ModifyEntityCommand : public Command {
public:
    ModifyEntityCommand(uint64_t entityId, const std::string& property,
                        const std::string& oldValue, const std::string& newValue);
    void Execute() override;
    void Undo() override;
    std::string Name() const override { return "Modify Entity"; }
private:
    uint64_t entityId_;
    std::string property_;
    std::string oldValue_;
    std::string newValue_;
};

} // namespace core
} // namespace workstation
