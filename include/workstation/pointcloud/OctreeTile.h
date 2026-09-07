#pragma once
#include "workstation/pointcloud/PointCloudNode.h"
#include "workstation/spatial/Octree.h"
#include <cstdint>
#include <array>
#include <string>

// ============================================================
// On-disk Octree Tile Format
// ============================================================
//
// Each tile stores a single octree node with all point attributes.
// Tiles are loaded from LAS/LAZ files on demand and uploaded to GPU.
//
// Tile layout on disk (bytes):
//   [0-7]   : nodeKey (uint64_t)
//   [8-43]  : boundingBox (BBox16: minX,f32 minY,f32 minZ,f32 maxX,f32 maxY,f32 maxZ,f32 = 4×12=48, but we use 5×4=20 for min+max floats...)
//            Actually: minX,f32 minY,f32 minZ,f32 maxX,f32 maxY,f32 maxZ,f32 = 4×12=48 bytes... let me reconsider
//   [44-47] : pointCount (uint32_t)
//   [48-51] : lodLevel (uint32_t)
//   [52-55] : diskOffset (int64_t low 4 bytes)
//   [56-59] : diskOffset (int64_t high 4 bytes) - actually use int32_t for 2GB tiles
//   [60]    : gpuBufferState (uint8_t)
//   [61]    : padding
//   [62-93] : reserved/future use
//   [94-95] : checksum (uint16_t)
//
// Point data in GPU buffer (per tile):
//   [0-11]   : position (float32 x, y, z) - 12 bytes per point
//   [12-15]  : color (uint8 r, g, b) - 3 bytes per point
//   [16-19]  : intensity (float32) - 4 bytes per point
//   [20]     : classification (uint8) - 1 byte per point
//   [21]     : return number (uint8) - 1 byte per point
//   Stride: 22 bytes per point (packed, no padding)
//
// Tile memory layout (CPU-side, after upload to GPU):
//   - Bounding box for frustum culling
//   - Point count for indirect draw command
//   - LOD level for shader selection
//   - GPU buffer vertex offset
// ============================================================

namespace workstation { namespace pointcloud {

// GPU buffer state tracking for a tile
enum class GPUBufState : uint8_t {
    NotLoaded = 0,      // Not yet uploaded to GPU
    CPUUploading,       // Currently being uploaded from CPU
    GPUResident,        // Successfully resident in GPU memory
    Evicted,            // Removed from GPU, can be reloaded
    MaxState
};

// Bounding box stored as 6 floats (minX, minY, minZ, maxX, maxY, maxZ)
struct BBox16 {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

// ============================================================
// OctreeTile - On-disk tile representation
// ============================================================
struct OctreeTile {
    // Tile identification
    uint64_t nodeKey;          // Unique ID (hash of bounds + depth, or stored offset)
    BBox16 bounds;             // Axis-aligned bounding box
    uint32_t pointCount;       // Number of points in this tile
    uint32_t lodLevel;         // Current LOD level (0=full detail, higher=simplified)
    int32_t diskOffset;        // File offset where tile data resides (-1 if not on disk)
    GPUBufState gpuState;      // Current GPU buffer state

    // Tile validity checksum
    uint16_t checksum;         // Simple CRC16 of tile data for integrity check

    // Constructor - initialize to invalid state
    OctreeTile() : nodeKey(0), pointCount(0), lodLevel(0),
                   diskOffset(-1), gpuState(GPUBufState::NotLoaded),
                   checksum(0) {}

    // Check if tile is valid (bounds are finite, pointCount > 0)
    bool IsValid() const {
        return pointCount > 0 &&
               !std::isnan(minX) && !std::isinf(minX) &&  // simplified check
               gpuState != GPUBufState::NotLoaded;
    }

    // Get GPU buffer vertex offset (assuming interleaved XYZRGBIR format)
    // Returns offset in bytes from start of tile's vertex buffer
    VkDeviceSize GetVertexOffset() const {
        // 22 bytes per point: xyz(12) + rgb(3) + intensity(4) + classification(1) + returnNum(1)
        return static_cast<VkDeviceSize>(pointCount) * 22;
    }

    // Get GPU buffer size in bytes
    VkDeviceSize GetBufferSize() const {
        return GetVertexOffset();
    }

    // Get total disk storage size including header and point data
    // Point format: XYZ32+RGB8+Intensity32+Classification8+ReturnNum8 = 22 bytes/point
    static size_t GetTileSizeBytes(uint32_t pointCount) {
        return sizeof(OctreeTile) + (22 * pointCount);
    }

    // Serialize tile to byte buffer for disk I/O
    bool Serialize(std::vector<uint8_t>& outData) const;

    // Deserialize tile from byte buffer
    bool Deserialize(const std::vector<uint8_t>& data);
};

// ============================================================
// OctreeTileManager - Manages on-disk tile storage and GPU residency
// ============================================================
class OctreeTileManager {
public:
    OctreeTileManager() = default;
    ~OctreeTileManager() = default;

    // Load tile from disk by file offset
    // Returns true if tile was successfully loaded
    bool LoadTileFromDisk(int32_t fileOffset, const std::string& filePath);

    // Unload tile from GPU (free buffer memory)
    void UnloadTileGPU();

    // Upload tile to GPU (async, returns immediately)
    // Returns true if upload was queued
    bool UploadTileGPU(VkDevice device, VkQueue uploadQueue,
                      const VkBufferUsageFlags usageFlags);

    // Check if tile is currently resident on GPU
    bool IsTileResident() const { return tile.gpuState == GPUBufState::GPUResident; }

    // Get the tile data
    const OctreeTile& GetTile() const { return tile; }

    // Get point count
    uint32_t GetPointCount() const { return tile.pointCount; }

    // Get bounds
    const BBox16& GetBounds() const { return tile.bounds; }

    // Get LOD level
    uint32_t GetLODLevel() const { return tile.lodLevel; }

    // Get node key
    uint64_t GetNodeKey() const { return tile.nodeKey; }

    // Mark tile as evicted from GPU
    void EvictTile() { tile.gpuState = GPUBufState::Evicted; }

    // Mark tile as GPU resident
    void MarkResident() { tile.gpuState = GPUBufState::GPUResident; }

    // Mark tile as CPU uploading
    void MarkUploading() { tile.gpuState = GPUBufState::CPUUploading; }

private:
    OctreeTile tile;           // The tile data
    std::string tileFilePath;  // Path to tile data file
    VkDevice gpuDevice;        // Vulkan device for buffer creation
    VkQueue uploadQueue;       // Queue for staging uploads
    VkBuffer gpuBuffer;        // GPU buffer holding point data
    VkDeviceMemory gpuMemory;  // GPU memory allocation
};

// ============================================================
// PointFormat - Point data format for tile storage
// ============================================================
enum class PointFormat : uint8_t {
    XYZRGBIR = 0,  // float32 XYZ + uint8 RGB + float32 Intensity + uint8 Classification + uint8 ReturnNum
    XYZRGBN,       // float32 XYZ + uint8 RGB + uint8 NormalPacked
    XYZI,          // float32 XYZ + float32 Intensity
    XYZC,          // float32 XYZ + uint8 Classification
    MaxFormat
};

// ============================================================
// PointAttribute - Single point attribute in GPU buffer
// Layout per point (22 bytes total, packed):
//   offset 0:   float32 x, float32 y, float32 z   (12 bytes)
//   offset 12:  uint8  r, uint8  g, uint8  b       (3 bytes)
//   offset 15:  float32 intensity                   (4 bytes)
//   offset 19:  uint8  classification               (1 byte)
//   offset 20:  uint8  return number                (1 byte)
// ============================================================
struct PointGPU {
    float x, y, z;     // Position (12 bytes)
    uint8_t r, g, b;   // Color (3 bytes)
    float intensity;   // Intensity (4 bytes)
    uint8_t classification;  // Classification (1 byte)
    uint8_t returnNum;       // Return number (1 byte)
};

// Static assertion to verify stride is 22 bytes
static_assert(sizeof(PointGPU) == 22, "PointGPU must be 22 bytes packed");

// ============================================================
// Tile Chunk Header - For chunked tile storage on disk
// Used when tiles are too large to load as single unit
// ============================================================
struct TileChunkHeader {
    uint32_t magic;          // TileChunkMagic = 0x54434B48 ('TCHK')
    uint32_t version;        // Format version
    uint32_t chunkIndex;     // Index within tile
    uint32_t chunkPointCount;// Points in this chunk
    uint32_t chunkOffset;    // Offset within tile's point data
    uint16_t checksum;       // CRC16 of chunk data
};

// Magic constant for tile chunks
constexpr uint32_t TileChunkMagic = 0x54434B48;

// ============================================================
// Tile Loading Flags
// ============================================================
enum class TileLoadFlags : uint32_t {
    None        = 0x0000,
    Mipmap      = 0x0001,  // Generate mipmaps for LOD levels
    Decompress    = 0x0002, // Decompress LAZ data
    Async         = 0x0004, // Initiate async read (non-blocking)
    FullDecode    = 0x0008 // Fully decode all channels (vs minimal)
};

// ============================================================
// OctreeTileManager - Static methods for tile operations
// ============================================================
class OctreeTileUtils {
public:
    // Encode tile to byte buffer
    static bool EncodeTile(const OctreeTile& tile, const PointGPU* points,
                          std::vector<uint8_t>& outBuffer);

    // Decode tile from byte buffer
    static bool DecodeTile(const std::vector<uint8_t>& data, OctreeTile& tile,
                           std::vector<PointGPU>& outPoints);

    // Compute CRC16 for data integrity
    static uint16_t ComputeCRC16(const uint8_t* data, size_t length);

    // Get point format size in bytes
    static size_t GetPointFormatSize(PointFormat format);

    // Get string description of GPUBufState
    static const char* GetGPUStateString(GPUBufState state);

    // Get string description of PointFormat
    static const char* GetPointFormatString(PointFormat format);
};

}} // namespace workstation::pointcloud