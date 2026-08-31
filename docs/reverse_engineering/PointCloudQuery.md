# Point Cloud Query Foundation — Reverse Engineering & Design (Piece 1)

Clean-room implementation of the reverse-engineered point cloud query
architecture. This document records **CONFIRMED** behaviour (from Ghidra)
separately from **UNKNOWN / INFERENCE**, plus our independent design decisions.

## CONFIRMED REVERSE ENGINEERING (implemented in Piece 1)

### High-level architecture (A)
Separate stages: Query creation → Spatial filtering → Point retrieval →
Point loading budget → Rendering. API layer (PtVortex) dynamically binds
functions; real logic lives in the backend/query engine.

### Dynamic API loading (B)
Functions are resolved through a wrapper layer (ptOpenPOD, ptCreateSceneInstance,
ptPtsToLoadInViewport, ptDrawGL). We model this as: Public API → Engine
Interface → Backend Implementation. `PointCloudEngine` is the engine singleton.

### Query architecture (C)
`PointCloudQueryManager` → `PointCloudFrustumQuery` → `PointQueryExecutor`
(ReadPoints) → `SpatialTree` → `PointBlock`s.

### Frustum query creation (D, E)
`ptCreateFrustumPointsQuery`:
1. atomic increment global counter → unique query ID
2. allocate query object
3. init: density=1.0, mode=2, active=false, pointers cleared
4. name = "FRUSTUM"
5. result storage, initial capacity 1024
6. register query object

### Frustum query execution (F)
`FUN_1800389d0`:
1. first execution → store start timestamp
2. prepare query execution context
3. create point reader object
4. build traversal state
5. spatial traversal
6. per candidate → visibility test
7. accepted → store in result container
8. update processed count / stats / progress
9. return processed count

### ReadPoints / PointQueryExecutor (G, L)
`querydetail::ReadPoints<Frustum>` (our `PointQueryExecutor`):
- bounds (BoundingBox), QueryContext, PointChannelManager, ResultBuffer
- `FUN_180045040` GetState(void*): reads internal state pointer; if it exists
  AND comparison != state → return `(state>>8)<<8`; otherwise
  `((state>>8)<<8) | 1` (valid flag in bit 0). **Implemented exactly.**

### Point channel system (H)
Up to 32 channels: XYZ, RGB, Intensity, Classification, Normals, additional.
`PointChannelManager` with `MAX_CHANNELS = 32`.

### Spatial tree (I)
`SpatialNode { left, right, key, pointCount }`. Traversal:
`if key < node.key -> left else -> right`. Matching nodes: accumulate pointCount.

### Point load budget (J)
`ptPtsToLoadInViewport`: NOT visibility. Convert handle → key, access tree,
traverse by key, accumulate point counts, store lastRequested, return total.

### Visibility test (K)
`FUN_180058e40` every candidate passes `VisibilityTest(node)` → accept/reject.
**Exact plane mathematics NOT yet reversed → abstraction only.**

## UNKNOWN REVERSE ENGINEERING (abstractions / not implemented)

- Exact octree structure (we implement the confirmed BST key-comparison).
- Exact frustum plane equations (TestVisibility is a placeholder accepting all).
- Exact POD format, point compression.
- GPU upload, DrawGL renderer, CUDA strategy, LOD formula, shader system.
- `convertHandle` external→internal key mapping is identity (pending RE).

## OUR DESIGN DECISIONS (clean-room, Rule #2)

| Original (proprietary) | Our implementation |
|-------------------------|--------------------|
| PtVortex / PointCloudCore | `PointCloudEngine` (Meyers singleton) |
| PointCloudQueryManager | `PointCloudQueryManager` (thread-safe) |
| FrustumQuery | `PointCloudFrustumQuery` |
| querydetail::ReadPoints<Frustum> | `PointQueryExecutor` |
| result vector | `QueryResult` (capacity 1024) |
| SpatialNode/SpatialTree | same independent names |

- No proprietary offsets reproduced; structs are clean C++.
- `SpatialNode` gains a `BoundingBox bounds` member (design decision) so a future
  frustum test has a volume; not a recovered field.
- `PointCloudFrustumQuery::BindTree` lets a query reference any spatial tree
  (engine tree by default), keeping tests independent of the singleton.
- Visibility placeholder returns `true`; replaceable without engine redesign.

## NEXT RECOMMENDED REVERSE ENGINEERING TARGET

Recover `FUN_180058e40` plane equations and the frustum parameter encoding
(four doubles at +0x5C..0x78) so `PointQueryExecutor::TestVisibility` can be
replaced with the real Bentley frustum intersection.
