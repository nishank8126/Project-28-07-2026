#pragma once
#include "workstation/math/Point3d.h"
#include "workstation/tools/ClassificationAlgorithms.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace workstation {

namespace pointcloud { class PointCloud; }

namespace tools {

struct ClassificationInfo {
    uint32_t code = 0;
    std::string name;
    bool visible = true;
    uint32_t color = 0xFFFFFFFF;
};

// nodeId here is a point index into the target cloud's single root node
// (see PointStorage::WriteClassification) - there's no per-point picking UI
// wired up to populate these yet, so AddNodeToEdit/ApplyEdits exist for a
// future manual-pick workflow; the automated Classify* algorithms below are
// the primary way points get reclassified today.
struct ClassificationEdit {
    uint64_t nodeId = 0;
    uint32_t oldClassification = 0;
    uint32_t newClassification = 0;
};

// One past algorithmic classification pass, kept so the UI can offer "Undo
// last" without re-deriving what changed.
struct AppliedAlgoResult {
    std::string label;
    ClassifyResult result;
};

class ClassificationTool {
public:
    ClassificationTool() = default;
    ~ClassificationTool() = default;

    void Initialize();

    // The cloud that manual edits and the automated algorithms operate on.
    // Wired by Renderer::SetPointCloud so this always tracks whatever's
    // currently loaded.
    void SetTargetCloud(pointcloud::PointCloud* cloud) { targetCloud_ = cloud; }
    pointcloud::PointCloud* GetTargetCloud() const { return targetCloud_; }

    // Called after any classification write so the caller can re-upload the
    // GPU classification buffer (Renderer::RefreshClassification).
    void SetRefreshCallback(std::function<void()> cb) { onClassificationChanged_ = std::move(cb); }

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

    // Runs synchronously on the calling (UI) thread - fine for the point
    // counts this app currently loads, but a genuinely large cloud would
    // block a frame; moving this to a worker thread is a reasonable follow
    // up if that becomes a problem in practice.
    void RunIsolatedPoints();
    void RunLowPoints();
    void RunGroundPTD();
    void UndoLastAlgorithm();

    void RenderUI();

private:
    uint32_t target_ = 0;
    uint32_t newClass_ = 0;
    std::vector<ClassificationEdit> pendingEdits_;
    std::vector<ClassificationEdit> appliedEdits_;
    std::unordered_map<uint32_t, ClassificationInfo> classifications_;

    pointcloud::PointCloud* targetCloud_ = nullptr;
    std::function<void()> onClassificationChanged_;

    IsolatedPointsParams isolatedParams_;
    LowPointsParams lowParams_;
    GroundPTDParams groundParams_;

    std::vector<AppliedAlgoResult> appliedAlgoResults_;
    std::string lastRunStatus_;

    void NotifyChanged();
};

} // namespace tools
} // namespace workstation
