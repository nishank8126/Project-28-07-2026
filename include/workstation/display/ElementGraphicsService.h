#pragma once
#include "workstation/display/ElementRef.h"
#include "workstation/display/CacheKeys.h"
#include "workstation/display/CachedGraphics.h"
#include "workstation/display/IElementGraphicsProvider.h"
#include "workstation/display/ViewContext.h"
#include <cstddef>

namespace workstation {

struct GraphicsResolveRequest {
    ElementRef&                     elementRef;
    const IElementGraphicsProvider& provider;
    GraphicsUnsizedKey             key;
    double                         currentViewMetric;
    double                         validityFactor;
    bool                           allowPersistentCache = true; // RE: persistent vs transient
};

// Whether a generated CachedGraphics is kept in the persistent element cache
// or used only for the current draw and then released. OUR modeling of the
// confirmed persistent-vs-temporary QvElem distinction (RE: _DrawCached).
enum class GraphicsRetention {
    Persistent,
    Transient
};

struct GraphicsResolveResult {
    CachedGraphicsHandle graphics;
    GraphicsRetention     retention;
};

struct GraphicsServiceStats {
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
    size_t graphicsBuilds = 0;
    size_t nestedDirectEmits = 0;
};

// High-level equivalent of ViewContext::GetCachedGeometry / CreateCacheElem
// (RE: 1/2/3). Orchestrates Piece 1 cache policy with Piece 2 graphics
// generation. Does NOT perform cache lookup itself; it delegates to
// FindCachedGraphics / SaveCachedGraphics. The Element never knows about the
// cache (architectural rule).
class ElementGraphicsService {
    GraphicsServiceStats m_stats;

public:
    // Returns the resolved graphics plus its retention. On a cache hit the
    // graphics is Persistent; on a miss it is Persistent only when
    // allowPersistentCache (the default) and generation produced graphics.
    GraphicsResolveResult ResolveForPresentation(ViewContext& view,
                                                 const GraphicsResolveRequest& request);

    // Backward-compatible Piece 2 entry point (delegates, returns graphics).
    CachedGraphicsHandle Resolve(ViewContext& view,
                                 const GraphicsResolveRequest& request);

    const GraphicsServiceStats& Stats() const { return m_stats; }
};

} // namespace workstation
