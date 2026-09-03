#pragma once
#include <string>
#include <memory>
#include <vector>

namespace workstation {
namespace core {

class Command {
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual std::string Name() const = 0;
    virtual bool CanMerge() const { return false; }
    virtual std::unique_ptr<Command> MergeWith(std::unique_ptr<Command> other) { return nullptr; }
};

} // namespace core
} // namespace workstation
