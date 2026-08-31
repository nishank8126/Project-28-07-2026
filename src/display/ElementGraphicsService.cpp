#include "workstation/display/ElementGraphicsService.h"
#include "workstation/display/GraphicsCache.h"
#include "workstation/display/GraphicsRecordingScope.h"
#include <stdexcept>

namespace workstation {

GraphicsResolveResult ElementGraphicsService::ResolveForPresentation(
        ViewContext& view, const GraphicsResolveRequest& req) {
    // ---- RECORDING MODE (recursive cache-recording guard, RE: 3) ----
    // If a parent cache-miss creation is already recording, DO NOT start a
    // nested cache-recording session. Bypass the retained cache entirely and
    // emit this element directly into the parent's active recorder. The nested
    // Resolve call therefore returns no standalone CachedGraphics and is, by
    // nature, transient for this session.
    if (view.isRecordingCachedGraphics) {
        ++m_stats.nestedDirectEmits;
        req.provider.EmitGraphics(*req.elementRef.GetElement(), view,
                                  *view.activeRecorder);
        return {nullptr, GraphicsRetention::Transient};
    }

    // ---- NORMAL MODE ----
    CachedGraphicsHandle hit =
        FindCachedGraphics(req.elementRef, req.key, req.currentViewMetric);
    if (hit) {
        ++m_stats.cacheHits;
        return {hit, GraphicsRetention::Persistent};  // already in cache
    }

    ++m_stats.cacheMisses;
    ++m_stats.graphicsBuilds;

    GraphicsRecorder recorder;
    recorder.BeginElement(req.elementRef.Id(),
                          req.elementRef.GetElement()->GeometryRevision());
    {
        GraphicsRecordingScope scope(view, recorder);
        try {
            req.provider.EmitGraphics(*req.elementRef.GetElement(), view,
                                      recorder);
        } catch (...) {
            // Scope restores ViewContext state; recorder is abandoned and
            // nothing is saved. Propagate after cleanup.
            throw;
        }
    }

    CachedGraphicsHandle g = recorder.EndElement();

    // Persistent vs transient (RE: _DrawCached). Transient graphics are built
    // but NOT entered into the element cache; they live only as long as the
    // caller's shared_ptr. This is OUR modeling of the confirmed distinction;
    // the exact proprietary eligibility policy is unknown.
    bool persisted = false;
    if (g && req.allowPersistentCache) {
        SaveCachedGraphics(req.elementRef, req.key, req.currentViewMetric,
                           req.validityFactor, g);
        persisted = true;
    }

    GraphicsRetention retention =
        persisted ? GraphicsRetention::Persistent : GraphicsRetention::Transient;
    return {g, retention};
}

CachedGraphicsHandle ElementGraphicsService::Resolve(ViewContext& view,
                                                     const GraphicsResolveRequest& req) {
    return ResolveForPresentation(view, req).graphics;
}

} // namespace workstation
