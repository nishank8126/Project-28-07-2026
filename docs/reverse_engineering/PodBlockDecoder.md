# POD Block Decoder & Integration — RE & Design (Piece 4)

Evidence-backed POD-specific block decoder that replaces Piece 3's generic
block interpretation with POD-specific decoding, normalization, geometry paths,
handler registry, and checked arithmetic. Builds on Piece 2 (PointCloud /
Node / Voxel / PointStorage / PointAttributeChannel) and Piece 3
(PointCloudStreamReader, reused for regression). CONFIRMED behaviour from
Ghidra is separated from DESIGN DECISION and UNKNOWN; no proprietary class
names, ABI, offsets, or memory layouts are reproduced.

## CONFIRMED REVERSE ENGINEERING (implemented in Piece 4)

### A. Normalization functions

**Channel-code normalization (FUN_18006e560):**
Values 1–7 map to themselves; everything else → 0.

**Format-code normalization (FUN_18006e630):**
Values 1–10 map to themselves; everything else → 0.

**Element-size table (FUN_18006e5d0):**

| Type | Bytes |
|------|-------|
| 1    | 4     |
| 2    | 8     |
| 3    | 0     |
| 4    | 1     |
| 5    | 1     |
| 6    | 2     |
| 7    | 2     |
| 8    | 4     |
| 9    | 4     |
| 10   | 8     |

Type 3 returns 0 bytes (confirmed; semantic meaning unknown).

### B. Checked arithmetic

`checkedMultiply` and `checkedAdd` perform overflow-safe `size_t` arithmetic
before any allocation. `CalculatePayloadSize` computes:

    payloadBytes = GetElementSize(normalizedType) * count * stride

where `stride` is the component count (NOT a byte offset). Overflow or invalid
type returns false. Type 3 (elementSize = 0) is valid and produces 0 bytes.

### C. POD block decoder main pipeline (FUN_180078840)

Stream layout (POD-specific, NOT the generic Piece 3 format):

```
tableCount   u32
nodeCount    u32
auxTable     u32[tableCount]      (skipped; semantics UNKNOWN)
per node:
  typeFlag    u32                  (0 = Normal, 1 = Hierarchical)
  geomPath    u32                  (0 = Float32, 1 = Float64)
  geometry    6×f32 or 6×f64      (min corner + max corner)
  flags       u32
  metaParam   u32
  meta data   metaParam×4 bytes   (if ≤ 0x0C) or 6×u64 (if > 0x0C)
  metaMode    u32
  srcNodeVal  u64                  (8-byte source identifier)
  channelCount u32
  per channel:
    channelCode  u32               (1=XYZ 2=RGB 3=Intensity … 7)
    formatCode   u32               (1–10; see table above)
    count        u32
    stride       u32               (component count)
    scale        3×f64
    offset       3×f64
    payload      GetElementSize×count×stride bytes
```

**Pipeline steps:**
1. Validate cloud state (must be empty).
2. Create 256 KB buffered reader (PodBinaryReader wrapping PodDataSource).
3. Read tableCount + nodeCount.
4. Read and discard auxiliary table (if non-zero).
5. Per node: read typeFlag + geomPath from stream, then delegate to
   PodNodeDecoder::DecodeNode (reads geometry, flags, metadata, sourceNodeValue).
6. Decode channels (read channelCount, then per channel: normalize codes,
   calculate payload via checked arithmetic, read payload, create
   PointAttributeChannel, attach to node).
7. Attach node to root VoxelNode.
8. Set root on cloud, finalize (aggregate pointCount / attributes / memory).
9. Post-load handler dispatch (stub — handler semantics UNKNOWN).

Implemented in `PodBlockDecoder`.

### D. Node decoder (FUN_1800764c0 / FUN_180081100)

Reads the node body from the stream:
- **Float32 geometry path:** 6×f32 → widened to doubles for BoundingBox.
- **Float64 geometry path:** 6×f64 directly into BoundingBox.
- **Flags:** u32 (semantic meaning UNKNOWN).
- **Extra metadata (FUN_180082b40):** If metaParam ≤ 0x0C, read metaParam×4
  bytes into rawSmallData; otherwise read 6×u64 into extendedData. A separate
  metaMode u32 is stored.
- **Source node value:** 8-byte u64 identifier.
- Node type dispatch: Hierarchical → VoxelNode (density 1.0); Normal →
  PointCloudNode.

Implemented in `PodNodeDecoder`.

### E. Handler registry (FUN_180077400)

Ordered byte-key tree for post-load handler dispatch. Lookup uses
`std::lower_bound` with a `memcmp`-based comparison: compare the common prefix
via `memcmp`; on tie, shorter key is "less". Insert maintains sorted order.
The handler semantics themselves are UNKNOWN; the registry infrastructure is
implemented but the dispatch call is deferred.

Implemented in `PodHandlerRegistry`.

### F. Buffered reader (PodBinaryReader)

Wraps `PodDataSource` (abstract `read`/`readSome`/`seek`/`tell`). 256 KB
buffer with sliding-window refill (preserves unread bytes at front, reads more
from source). Provides `readU32`, `readU64`, `readFloat`, `readDouble`.

Implemented in `PodBinaryReader`.

### G. Channel decoder (PodChannelDecoder)

Normalizes raw channel/format codes from the stream, validates them, computes
payload size via checked arithmetic, reads the payload bytes, maps normalized
codes to `ChannelId` and `PointFormat`, and creates the `PointAttributeChannel`
via `Create()` (omitting the stream's stride — `Create()` derives byte stride
from `elementSize` automatically).

Implemented in `PodBlockDecoder::decodeChannels`.

## DESIGN DECISIONS (clean-room, Rule #2)

- Independent class names: `PodBlockDecoder`, `PodNodeDecoder`,
  `PodHandlerRegistry`, `PodBinaryReader`, `PodDataSource`, `PodPostLoadHandler`.
- `PodDataSource` is an abstract pull interface (no LAZ/POD reader).
- The POD binary layout above is our own reconstruction, not the proprietary
  format; field order and semantics are confirmed where possible.
- Node hierarchy: all decoded nodes are attached flat to a synthesized root
  VoxelNode (consistent with Piece 3). Real parent-child relationships from the
  POD stream are not yet reversed.
- Handler registry is implemented structurally (sorted vector + lower_bound
  memcmp) but the dispatch call is deferred (handler semantics UNKNOWN).
- `PodBlockDecoder` owns the root VoxelNode (and all child nodes) for the
  decoder's lifetime; the cloud holds raw node pointers via `SetRoot`.
- `CalculatePayloadSize` stride parameter = component count (e.g. 3 for XYZ),
  not a byte offset. This matches the confirmed formula.

## UNKNOWN (not implemented)

- POD decompression / LAZ / LASzip.
- COPC, octree, LOD selection, streaming, GPU upload, renderer.
- Auxiliary table semantics (tableCount / auxTable read and discarded).
- Handler registry dispatch (processHandlerTable deferred).
- Post-load metadata stages (FUN_1800774c0 / FUN_180078620 / FUN_180077c80).
- Special geometry codes 0x35F4C / 0x2748A (semantic meaning unknown).

## VALIDATION
`wk_pod_tests` (TEST 1–20) passes; full CTest suite (8/8) green. Pieces 1–3
unaffected.
