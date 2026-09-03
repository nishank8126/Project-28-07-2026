#include "workstation/core/Commands.h"

namespace workstation {
namespace core {

CreateLineCommand::CreateLineCommand(CadLineGeometry line)
    : line_(std::move(line)) {}

void CreateLineCommand::Execute() {
    executed_ = true;
}

void CreateLineCommand::Undo() {
    executed_ = false;
}

CreatePolylineCommand::CreatePolylineCommand(CadPolylineGeometry poly)
    : poly_(std::move(poly)) {}

void CreatePolylineCommand::Execute() {
    executed_ = true;
}

void CreatePolylineCommand::Undo() {
    executed_ = false;
}

CreateCircleCommand::CreateCircleCommand(CadCircleGeometry circle)
    : circle_(std::move(circle)) {}

void CreateCircleCommand::Execute() {
    executed_ = true;
}

void CreateCircleCommand::Undo() {
    executed_ = false;
}

CreatePolygonCommand::CreatePolygonCommand(CadPolygonGeometry polygon)
    : polygon_(std::move(polygon)) {}

void CreatePolygonCommand::Execute() {
    executed_ = true;
}

void CreatePolygonCommand::Undo() {
    executed_ = false;
}

CreateMeasurementCommand::CreateMeasurementCommand(MeasurementGeometry meas)
    : meas_(std::move(meas)) {}

void CreateMeasurementCommand::Execute() {
    executed_ = true;
}

void CreateMeasurementCommand::Undo() {
    executed_ = false;
}

ChangeClassificationCommand::ChangeClassificationCommand(
    std::vector<ClassificationChange> changes)
    : changes_(std::move(changes)) {}

void ChangeClassificationCommand::Execute() {
    for (auto& c : changes_) {
        std::swap(c.oldClass, c.newClass);
    }
}

void ChangeClassificationCommand::Undo() {
    for (auto& c : changes_) {
        std::swap(c.oldClass, c.newClass);
    }
}

MoveEntityCommand::MoveEntityCommand(uint64_t entityId,
                                       math::Point3d oldPos,
                                       math::Point3d newPos)
    : entityId_(entityId), oldPos_(oldPos), newPos_(newPos) {}

void MoveEntityCommand::Execute() {
    std::swap(oldPos_, newPos_);
}

void MoveEntityCommand::Undo() {
    std::swap(oldPos_, newPos_);
}

DeleteEntityCommand::DeleteEntityCommand(uint64_t entityId)
    : entityId_(entityId) {}

void DeleteEntityCommand::Execute() {
}

void DeleteEntityCommand::Undo() {
}

ModifyEntityCommand::ModifyEntityCommand(uint64_t entityId,
                                           const std::string& property,
                                           const std::string& oldValue,
                                           const std::string& newValue)
    : entityId_(entityId), property_(property), oldValue_(oldValue), newValue_(newValue) {}

void ModifyEntityCommand::Execute() {
    std::swap(oldValue_, newValue_);
}

void ModifyEntityCommand::Undo() {
    std::swap(oldValue_, newValue_);
}

} // namespace core
} // namespace workstation
