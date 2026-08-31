# Point Cloud Scene / Storage — Reverse Engineering & Design (Piece 2)

Clean-room implementation of the reverse-engineered point cloud data model:
Scene → Cloud → Node hierarchy → Channels → Point storage. CONFIRMED behaviour
(from Ghidra) is separated from DESIGN DECISION and UNKNOWN. No proprietary code,
offsets, ABI, or class names are reproduced (clean-room Rule #2).

## CONFIRMED REVERSE ENGINEERING (implemented in Piece 2)

### Channel element-size table (FUN_18006e2f0)
Format type code → fixed element size in bytes:

| Type code | bytes |
|-----------|-------|
| Type1     | 4     |
| Type2     | 8     |
| Type4     | 1     |
| Type5     | 1     |
| Type6     | 2     |
| Type7     | 2     |
| Type8     | 4     |
| Type9     | 4     |
| Type10    | 8     |

Implemented as `GetElementSize(PointChannelType)`.

### Channel creation (FUN_18006e2f0)
`CreateChannel`: determine element size, allocate `count * stride` storage,
copy the source buffer, store scale/offset (default transform when missing).
Implemented in `PointAttributeChannel::Create` / free `CreateChannel`.

### XYZ decode (FUN_1800820b0)
Reads FORMAT 1 (int16) and FORMAT 2 (float32); 3 components per point.
`output = input * scale + offset` (per axis). Implemented in
`PointAttributeChannel::ReadXYZ` / `PointStorage::ReadXYZ`.

### RGB decode (FUN_1800821f0)
Reads 3 x uint8 (R,G,B). Implemented in `PointAttributeChannel::ReadRGB` /
`PointStorage::ReadRGB`.

### Node / Voxel
- `VoxelNode` (FUN_180081100) is a specialized `PointCloudNode`: default
  `density = 1.0`, has a child container, a mutex, and a loading-state flag
  (reset by default).
- `PointCloudNode` (FUN_1800764c0 leaf) holds bounds + channels + owner cloud.
- Children are ONLY manually assigned; no subdivision/octree generation.

### Cloud finalize (FUN_18007d990)
Assign root, set owner on every node, accumulate `pointCount`, collect attribute
availability flags, and compute a simple memory statistic. Implemented in
`PointCloud::Finalize` (free `FinalizeCloud`).

### Scene
Holds multiple clouds; the workstation tracks a single active cloud.

## DESIGN DECISIONS (clean-room, Rule #2)

| Original (proprietary) | Our implementation |
|------------------------|--------------------|
| pcloud::Node           | `PointCloudNode`   |
| pcloud::Voxel          | `VoxelNode`        |
| DgnPointCloud / Cloud  | `PointCloud`       |
| Scene                  | `core::PointCloudScene` (singleton) |
| channel type enum      | `PointChannelType` + `GetElementSize` |
| channel storage        | `PointAttributeChannel` (owned buffer) |
| storage decode         | `PointStorage`     |

- `spatial::BoundingBox` is reused (aliased into `pointcloud`); no duplicate.
- `PointAttributeChannel` owns its buffer (`std::vector<uint8_t>`); the Piece 1
  `PointChannel` descriptor (4 members) is kept untouched for the query layer.
- Channel element size for XYZ/RGB/Normals is derived from format × component
  count (3 comps; int16=2B, float=4B, uint8=1B). The RE `GetElementSize` table
  is implemented separately and used by the future importer. (The type-code→
  high-level ChannelId mapping is NOT reversed, so importer wiring is pending.)
- `CreateChannel` source is assumed tightly packed (`count * elementSize`); the
  allocated buffer is `count * stride` (stride defaults to elementSize).
- Default scale/offset when omitted is the identity transform (scale=1,offset=0).
- Voxel child access guarded by `std::mutex`; loading-state flag is a bool.
- Finalize memory statistic is a simple sum of channel byte sizes (not the
  proprietary calculation).

## UNKNOWN (not implemented)
- POD / LAZ / LASzip reader, point compression.
- Voxel subdivision / octree generation, streaming loader.
- GPU buffers, rendering.

## VALIDATION
`wk_pc2_tests` (TEST 1–11) passes; full CTest suite (6/6) green; Piece 1
unaffected.
