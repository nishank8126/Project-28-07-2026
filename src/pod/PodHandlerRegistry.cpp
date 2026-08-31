#include "workstation/pod/PodHandlerRegistry.h"

namespace workstation { namespace pod {

void PodHandlerRegistry::insert(HandlerKey key, PodBlockHandler* handler) {
    tree_.insert(std::move(key), handler);
}

PodBlockHandler* PodHandlerRegistry::findLowerBound(
    std::span<const std::uint8_t> key) const {
    return tree_.lowerBound(key);
}

}} // namespace workstation::pod
