# ADR-005 — View-output abstraction boundary

- Status: ACCEPTED
- Date: Piece 3

## Context
Reverse engineering (`DrawQvElem`, `_DrawQvElem`, export surface, RE E27-E29)
shows a clear boundary between the ViewContext/cache logic and a downstream
output sink that receives retained graphics (push transform/clip, apply
overrides, submit along one of two paths). The original code talks to a
`ViewOutput`-like object.

## Observed original technique
ViewContext drives drawing but delegates the actual retained submission to a
ViewOutput-style object via PushTransClip / PopTransClip and submit calls.

## Our implementation choice
An `IViewOutput` abstract interface in the display layer, with NO D3D11
implementation. `RetainedGraphicsPresenter` depends only on `IViewOutput`, so
the future renderer can be supplied later without touching cache/generation
code. This keeps the brief's required layering (display depends on core;
renderer depends on display; core never depends on D3D).

## Reason
Preserves the recovered boundary and keeps the renderer swappable. Production
code uses independent names; only the concept maps to the reversed boundary,
documented in `docs/reverse_engineering/EvidenceLedger.md`.

## Possible future replacement
When Piece 8/9 arrives, a concrete `D3D11ViewOutput : IViewOutput` implements
the submission against real GPU resources. The interface is stable; only the
concrete class is added.
