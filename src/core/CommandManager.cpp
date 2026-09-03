#include "workstation/core/CommandManager.h"
#include <algorithm>

namespace workstation {
namespace core {

void CommandManager::Execute(std::unique_ptr<Command> command) {
    if (!command) return;

    if (undoIndex_ < static_cast<int>(history_.size())) {
        history_.erase(history_.begin() + undoIndex_, history_.end());
    }

    command->Execute();

    if (!history_.empty() && history_.back()->CanMerge()) {
        auto merged = history_.back()->MergeWith(std::move(command));
        if (merged) {
            history_.back() = std::move(merged);
            if (historyChangedCb_) historyChangedCb_();
            return;
        }
    }

    history_.push_back(std::move(command));
    ++undoIndex_;

    if (static_cast<int>(history_.size()) > MAX_HISTORY) {
        history_.erase(history_.begin());
        --undoIndex_;
    }

    if (historyChangedCb_) historyChangedCb_();
}

void CommandManager::Undo() {
    if (!CanUndo()) return;
    --undoIndex_;
    history_[undoIndex_]->Undo();
    if (historyChangedCb_) historyChangedCb_();
}

void CommandManager::Redo() {
    if (!CanRedo()) return;
    history_[undoIndex_]->Execute();
    ++undoIndex_;
    if (historyChangedCb_) historyChangedCb_();
}

void CommandManager::Clear() {
    history_.clear();
    undoIndex_ = 0;
    if (historyChangedCb_) historyChangedCb_();
}

std::string CommandManager::GetUndoActionName() const {
    if (!CanUndo()) return "";
    return history_[undoIndex_ - 1]->Name();
}

std::string CommandManager::GetRedoActionName() const {
    if (!CanRedo()) return "";
    return history_[undoIndex_]->Name();
}

} // namespace core
} // namespace workstation
