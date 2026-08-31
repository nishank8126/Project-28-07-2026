# WorkstationCAD — Architecture Overview

Independent clean-room CAD workstation. This document records the *intended*
architecture and the constraints that must hold as the project grows. It is
OUR design, informed by (but not copied from) reverse-engineered evidence
recorded in `docs/reverse_engineering/EvidenceLedger.md`.

## Dependency direction (permanent rule, RE S)
    Core        -> (nothing above it)
    Geometry    -> Core
    Display     -> Core + Geometry
    Renderer    -> Display
    GUI         -> public application APIs

Core and Display MUST NOT depend on Direct3D. GPU objects live only in the
Renderer/Display resource layer; `CachedGraphics` (Piece 1) deliberately holds
no GPU state.

## Ownership backbone (Piece 1)
    Document
      owns -> Model(s)
                owns -> Element(s)            (CAD meaning)
    ElementRef (display layer)
      references -> Element (non-owning)
      owns       -> retained graphics cache
                      - direct common-variant slots (fast path, RE C)
                      - specialized variant set (RE D/H)

NOTE: For Piece 1 the `ElementRef` lives in the display layer and is created
by the display side given an `Element*`. The brief says "Model owns
element references"; the precise hosting of `ElementRef` (Model vs View) will
be finalized when the View/selection subsystems arrive (Piece 6/11). The
cache behavior is independent of that decision.

## Cache model
Two-tier per-element retained graphics cache:

1. **Direct (simple) path** — key has no style, no filter, transform key 0,
   validity factor 0. Stored in `ElementRef::m_direct[variant]` (cheap,
   indexable, no hash lookup). See ADR-001/002.

2. **Specialized path** — key carries style/filter/transform state and/or a
   metric validity range. Stored in `ElementGraphicsVariantSet`, sorted
   ascending by `minViewMetric`. Lookup validates transform key, variant,
   style key, filter key, and `metric in [minViewMetric, maxViewMetric]`.
   Overlapping ranges are allowed; the tightest matching range wins.

View-metric validity range implements LOD/tessellation reuse: small zoom
changes inside the range are cache hits and do NOT retessellate (RE F).

## Invalidation (our initial policy, RE UNKNOWN)
`ElementRef::InvalidateGraphics()` clears both direct and specialized slots.
Additionally, every lookup compares the stored `SourceGeometryRevision`
against the element's current `GeometryRevision()`; a mismatch is a cache
miss. Geometry revision is advanced via `Element::TouchGeometry()`. The exact
proprietary invalidation granularity is still under reverse engineering and
may refine this policy without an architecture change.

## Performance constraints (permanent)
- No full scene rebuild for camera movement.
- No full scene rebuild for ordinary display-mode changes (RE P).
- No unnecessary retessellation; preserve element-level invalidation.
- Reuse retained graphics; support multiple view-metric representations.
- Keep the hot common cache path cheap (direct slots, RE C).

## Implemented pieces
- Piece 1: Core backbone + Element retained graphics cache. [DONE]
- Piece 2: Element graphics generation + cache recording pipeline
  (`IElementGraphicsProvider`, `GraphicsRecorder`, `GraphicsRecordingScope`,
  `ViewContext`, `ElementGraphicsService`). [DONE] — see
  `GraphicsGeneration.md`.
- Piece 3: Retained graphics view-output submission layer
  (`IViewOutput`, `ViewSubmissionScope`, `ElementOverrideScope`,
  `ElementRenderOverrides`, `RetainedGraphicsPresenter`,
  `ElementPresentationService`, persistent/transient retention). [DONE] — see
  `RetainedSubmission.md`.

## Future pieces (order may change with new evidence)
2: graphics producer/stroker + recorder
3: math foundation
4: basic CAD geometry
5: tessellation/faceting
6: retained scene + display representations
7: ViewContext/Viewport/Camera/DisplayStyle
8: D3D11 device + GPU resource manager
9: initial geometry rendering
10: deferred/G-buffer rendering
11: selection/picking
12: snapping/measurement/editing
13: transactions/undo-redo
14: levels/materials/display properties
15: Qt 6 GUI
16: model tree / properties / panels
17: advanced geometry (B-splines/surfaces/solids)
18: point clouds
19: raster
20: import/export
21: plugin API
Final: Python automation bindings
