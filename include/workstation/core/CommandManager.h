#pragma once
#include "workstation/core/Command.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace workstation {
namespace core {

class CommandManager {
public:
    CommandManager() = default;
    ~CommandManager() = default;

    void Execute(std::unique_ptr<Command> command);

    bool CanUndo() const { return undoIndex_ > 0; }
    bool CanRedo() const { return undoIndex_ < static_cast<int>(history_.size()); }

    void Undo();
    void Redo();

    void Clear();

    std::string GetUndoActionName() const;
    std::string GetRedoActionName() const;

    uint32_t GetHistorySize() const { return static_cast<uint32_t>(history_.size()); }
    int GetUndoIndex() const { return undoIndex_; }

    using HistoryChangedCallback = std::function<void()>;
    void SetHistoryChangedCallback(HistoryChangedCallback cb) { historyChangedCb_ = std::move(cb); }

    static constexpr int MAX_HISTORY = 100;

private:
    std::vector<std::unique_ptr<Command>> history_;
    int undoIndex_ = 0;
    HistoryChangedCallback historyChangedCb_;
};

} // namespace core
} // namespace workstation
