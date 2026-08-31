# ADR-004 — Minimal recorded-command format

- Status: ACCEPTED
- Date: Piece 2

## Context
Piece 2 needs *some* representation of "graphics emitted during generation" to
validate the recording pipeline (command count, nesting into parent recorder).
The final geometry packet format (`DrawPacket` / retained scene, Piece 6) is
not yet designed.

## Observed original technique
Unknown. The original recorded/produced graphics objects are retained QVision
elements; their exact packet layout has not been reversed.

## Our implementation choice
`RecordedGraphicsCommand { uint32_t type; uint64_t payload; }` held in a
`std::vector` inside `GraphicsRecorder`. It carries no geometry meaning yet —
only enough to count and nest commands deterministically in tests.

## Reason
Keeps Piece 2 focused on the orchestration/caching behavior (the actual goal)
instead of prematurely designing geometry packets. The recorder's public
surface (`BeginElement` / `EmitCommand` / `EndElement`) is independent of this
internal representation, so the command format can be replaced wholesale later
without touching `ElementGraphicsService`, `IElementGraphicsProvider`, or the
cache.

## Possible future replacement
Replace the internal vector with a real `DrawPacket` / retained scene buffer
(Piece 6) produced by the recorder. `EndElement` will then return a
`CachedGraphics` backed by that packet instead of a command count. Public APIs
stay stable.
