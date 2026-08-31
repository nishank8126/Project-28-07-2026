# Retained Submission Layer (Piece 3)

Independent clean-room equivalent of `ViewContext::_DrawCached`,
`_DrawQvElem`, and `DrawQvElem` (RE E26-E29). Renderer-independent: there is
no D3D11 implementation yet. The dependency direction preserved:

    core
      |
    display/cache (Piece 1 + 2)
      |
    view submission (Piece 3)
      |
    future renderer (IViewOutput implementation)

## Data flow
```
Element
   |
   v
ElementGraphicsService :: ResolveForPresentation
   |  (cache hit / miss + generate, persistent or transient)
   v
GraphicsResolveResult { graphics, retention }
   |
   v
ElementPresentationService :: Draw   (wires resolve -> present)
   |
   v
RetainedGraphicsPresenter :: Present
   |
   +-- ViewSubmissionScope  -> output.PushTransformClip
   |
   +-- ElementOverrideScope -> output.ApplyElementOverrides(temp)   (if augmented)
   |
   +-- Submit Path A / Path B   (PathB receives view.retainedSubmissionParameter)
   |
   +-- ElementOverrideScope dtor -> output.ApplyElementOverrides(original)
   |
   +-- ViewSubmissionScope dtor -> output.PopTransformClip
   |
   v
IViewOutput  (future renderer)
```

## Persistent vs transient
```
PERSISTENT
    Element cache
        |
    CachedGraphics
        |
    submit repeatedly (retrieved on later resolves)

TRANSIENT
    build (allowPersistentCache = false)
        |
    submit once
        |
    shared_ptr released -> never stored in element cache
```
The distinction (RE E26) is modeled with shared_ptr ownership instead of
manual QvElem destruction. OUR policy: `GraphicsResolveRequest::
allowPersistentCache` selects; the exact proprietary eligibility rules are
unknown, so we expose the choice explicitly rather than guessing them.

## New types (Piece 3)
- `TransformToken`, `ClipVolumeToken` (opaque/replaceable placeholders for the
  later math/geometry piece).
- `ElementRenderOverrides` (typed optional lineColor/fillColor/materialToken/
  auxiliaryValue; NOT the proprietary bitmask).
- `IViewOutput` (PushTransformClip/PopTransformClip/ApplyElementOverrides/
  SubmitRetainedPathA/SubmitRetainedPathB).
- `ViewSubmissionScope`, `ElementOverrideScope` (RAII).
- `RetainedSubmissionPath`, `RetainedSubmissionRequest`, `PresentationStats`.
- `RetainedGraphicsPresenter::Present`.
- `ElementPresentationService::Draw`.
- `GraphicsRetention`, `GraphicsResolveResult` (in ElementGraphicsService).

## Exception safety
Both scopes restore state in their destructors (even on throw), so a failing
submit still leaves the ViewContext and output balanced (no leaked transform/
clip push, original overrides restored). `RetainedGraphicsPresenter` does not
increment its counters if submission throws.
