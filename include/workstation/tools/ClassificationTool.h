#pragma once
#include "workstation/math/Point3d.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace workstation {
namespace tools {

struct ClassificationInfo {
    uint32_t code = 0;
    std::string name;
    bool visible = true;
    uint32_t color = 0xFFFFFFFF;
};

struct ClassificationEdit {
    uint64_t nodeId = 0;
    uint32_t oldClassification = 0;
    uint32_t newClassification = 0;
};

class ClassificationTool {
public:
    ClassificationTool() = default;
    ~ClassificationTool() = default;

    void Initialize();

    void SetTargetClassification(uint32_t code) { target_ = code; }
    uint32_t GetTargetClassification() const { return target_; }

    void SetNewClassification(uint32_t code) { newClass_ = code; }
    uint32_t GetNewClassification() const { return newClass_; }

    void AddNodeToEdit(uint64_t nodeId, uint32_t oldClassification);
    void ClearEdits();
    void ApplyEdits();
    void UndoEdits();

    const std::vector<ClassificationEdit>& GetPendingEdits() const { return pendingEdits_; }
    const std::vector<ClassificationEdit>& GetAppliedEdits() const { return appliedEdits_; }
    const std::unordered_map<uint32_t, ClassificationInfo>& GetClassifications() const { return classifications_; }

    bool HasPendingEdits() const { return !pendingEdits_.empty(); }

    void RenderUI();

private:
    uint32_t target_ = 0;
    uint32_t newClass_ = 0;
    std::vector<ClassificationEdit> pendingEdits_;
    std::vector<ClassificationEdit> appliedEdits_;
    std::unordered_map<uint32_t, ClassificationInfo> classifications_;
};

} // namespace tools
} // namespace workstation
