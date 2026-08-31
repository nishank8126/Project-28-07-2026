# Graphics Generation Pipeline (Piece 2)

Independent clean-room equivalent of the architecture observed in
`ViewContext::GetCachedGeometry`, `ViewContext::CreateCacheElem`, and the
`IStrokeForCache`-role graphics producer (RE findings 1-5, 11, 12; recorded as
E20-E24 in the Evidence Ledger).

## Dependency direction (intentional)
    Element                  = CAD / model meaning (core)
    IElementGraphicsProvider = conversion CAD meaning -> display output (role only)
    GraphicsRecorder         = retained graphics construction (records commands)
    ElementGraphicsService   = cache-resolution orchestration
    ElementGraphicsCache     = retained-cache policy (Piece 1: Find/Save/Invalidate)

The Element NEVER queries the cache. The cache layer never depends on the
geometry producer. The producer depends only on Element + ViewContext +
GraphicsRecorder. This keeps the brief's required separation (RE S).

## Normal-mode resolve
```
Element
  |
  v
ElementGraphicsService::Resolve(view, request)
  |
  +-- FindCachedGraphics (Piece 1)
  |         |
  |      HIT -----> return cached graphics (provider NOT invoked)
  |
  |      MISS
  v
GraphicsRecorder
  BeginElement(elementId, geometryRevision)
  |
  v
GraphicsRecordingScope  (RAII: sets view.isRecordingCachedGraphics=true,
                         view.activeRecorder=&recorder; saves display params)
  |
  v
IElementGraphicsProvider::EmitGraphics(element, view, recorder)
  |                         emits RecordedGraphicsCommand(s)
  v
GraphicsRecordingScope dtor  (restores view state + display params)
  |
  v
GraphicsRecorder::EndElement()  -> CachedGraphicsHandle (or nullptr if empty)
  |
  v
SaveCachedGraphics (Piece 1)  [only if non-null]
  |
  v
return CachedGraphics
```

## Recording-mode (nested) resolve
When `view.isRecordingCachedGraphics == true`, a Resolve call is a NESTED call
made from inside a parent producer. It must NOT start its own cache-recording
session:
```
Parent recording session active
  |
  v
provider calls ElementGraphicsService::Resolve(view, childRequest)
  |
  v
view.isRecordingCachedGraphics == true
  |
  +-- bypass Piece 1 cache lookup
  +-- bypass new GraphicsRecorder creation
  +-- invoke childProvider.EmitGraphics(view, *view.activeRecorder)
  |        (child commands go into PARENT recorder)
  v
return nullptr   (no standalone child cache entry this session)
```
This models the recursive-lookup guard seen in `GetCachedGeometry` (RE E23):
nested CAD components do not recursively create independent cache-recording
sessions. After the parent session ends, the child is not independently cached
(until someone resolves it in normal mode).

## Lifecycle / exception safety
- `GraphicsRecorder` rejects invalid usage: Begin twice, End without Begin,
  Emit outside recording (throws `std::logic_error`).
- `GraphicsRecordingScope` restores ViewContext + display params in its
  destructor, even when the producer throws. No partially-built CachedGraphics
  is saved (the recorder is abandoned).
- Copy/move of `GraphicsRecordingScope` are deleted: exactly one scope per
  stack frame, reinforcing the no-nested-recording rule.

## CachedGraphics remains renderer-independent
`CachedGraphics` holds no D3D/GPU objects (RE rule 8 / S). The recorded
commands are a stand-in (`RecordedGraphicsCommand { type, payload }`) until the
real geometry packet (`DrawPacket`) is designed in a later piece.

## Statistics (no global singleton)
`ElementGraphicsService` exposes `GraphicsServiceStats { cacheHits,
cacheMisses, graphicsBuilds, nestedDirectEmits }` via `Stats()`. Useful for
later instrumentation of cache effectiveness.

## New types introduced (Piece 2)
- `RecordedGraphicsCommand`
- `ElemDisplayParams` (placeholder)
- `ViewContext` (minimal, renderer-independent)
- `GraphicsRecorder`
- `IElementGraphicsProvider`
- `GraphicsRecordingScope` (RAII)
- `ElementGraphicsService` (+ `GraphicsResolveRequest`, `GraphicsServiceStats`)
