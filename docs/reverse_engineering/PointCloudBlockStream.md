# Point Cloud Block Streaming & Scene Population — RE & Design (Piece 3)

Clean-room implementation of the generic binary point block stream → parser →
node/channel creation → cloud population pipeline. Builds on Piece 2
(PointCloud/Node/Voxel/PointStorage/PointAttributeChannel). CONFIRMED behaviour
(from Ghidra) is separated from DESIGN DECISION and UNKNOWN; no proprietary
class names, ABI, offsets, or memory layouts are reproduced.

## CONFIRMED REVERSE ENGINEERING (implemented in Piece 3)

### A. Buffered stream reader (FUN_18006d760)
- Current position, buffer start/end, source reader.
- Refill algorithm: check remaining; if insufficient, request more; preserve
  unread bytes; grow buffer; copy remaining; read additional from source.
- Confirmed buffer window size: **0x40000 (256 KB)**.
Implemented in `PointCloudStreamReader` (with `PointBlockSource` abstract source).

### B. Cloud block loading pipeline (FUN_180078840) — LoadCloudBlocks
1. Validate cloud state. 2. Create buffered stream reader. 3. Read block count.
4. Read node index table. 5. Per block: parse node descriptor, create node,
load channels, attach node. 6. Finalize cloud (FUN_18007d990).
Implemented in `CloudBlockLoader::Load`.

### C/D. Node & Voxel creation
- Normal block → `PointCloudNode` (bounds + channels).
- Hierarchical block → `VoxelNode` (base node; child container; mutex;
  density = 1.0; bounds; channels cleared). No automatic subdivision.

### E. Node descriptor (abstraction)
`NodeDescriptor { BoundingBox bounds; uint32_t flags; uint32_t type; }`.

### F/G. Channel loading (FUN_180081480)
Per node: `CreateChannel` (Piece 2 `PointAttributeChannel`): allocate storage,
copy payload, attach channel. Channel descriptor = type, count, stride, scale,
offset, data.

### H. Finalize (FUN_18007d990)
Assign root; set owner on each node; accumulate pointCount; compute attribute
availability; update memory statistics. Uses `PointCloud::Finalize`.

## DESIGN DECISIONS (clean-room, Rule #2)
- Independent class names: `PointCloudStreamReader`, `PointBlockParser`,
  `NodeDescriptor`, `ChannelDescriptor`, `CloudBlockLoader`.
- `PointBlockSource` is an abstract pull interface (no LAZ/POD reader).
- A clean, self-described binary layout was defined for the generic stream
  (block count u32 → index table u32[count] → per block: type u32, bounds
  6×f64, flags u32, channelCount u32, channels[ id u32, format u32, count u32,
  stride u32, scale 3×f64, offset 3×f64, payload count*stride bytes ]). This is
  our own format, not the proprietary one.
- Hierarchy: the node index table / parent-child relationship was NOT reversed;
  blocks are attached flat to a synthesized root `VoxelNode` (DESIGN DECISION).
  The root is a container; real parent links are pending RE.
- `CloudBlockLoader` owns the synthesized root (and thus all nodes) for the
  lifetime of the loader; the cloud holds raw node pointers set via `SetRoot`.
- `Load` refuses to overwrite an already-populated cloud (validation step 1).

## UNKNOWN (not implemented)
- LAS/LAZ/POD/COPC parsers, decompression, compression.
- Voxel subdivision, LOD selection, streaming GPU upload, renderer.

## VALIDATION
`wk_pc3_tests` (TEST 1–9) passes; full CTest suite (7/7) green. Pieces 1–2
unaffected.
