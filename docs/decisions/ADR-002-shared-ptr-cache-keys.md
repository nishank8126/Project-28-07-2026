# ADR-002 — Use shared_ptr for cached graphics handles

- Status: ACCEPTED
- Date: Piece 1

## Context
Reverse engineering (finding J) shows the original code AddRefs/Releases
style and filter key objects (and by extension the retained graphics
references). The proprietary ref-count ABI is not something we may or should
reproduce.

## Observed original technique
Manual AddRef/Release COM-style reference counting on key/cache objects.

## Our implementation choice
- `CachedGraphicsHandle = std::shared_ptr<CachedGraphics>` for retained
  graphics references owned/shared by the cache.
- `DisplayStyleCacheKey` / `DisplayFilterCacheKey` are immutable value types
  stored by value inside `GraphicsUnsizedKey` (`std::optional`).

## Reason
`std::shared_ptr` gives safe, correct lifetime management with zero
proprietary coupling. Keys are small opaque comparable values; copying them
by value is cheap and avoids any shared mutable state. This satisfies the
brief ("Choose the safest clean implementation initially. Do NOT imitate the
proprietary ref-count ABI.").

## Possible future replacement
If profiling demands it, retained graphics handles could move to an intrusive
reference count or a slot-based handle table inside a future slab allocator
(ADR-001), without changing call sites that already use `CachedGraphicsHandle`.
