#pragma once
#include "workstation/pointcloud/PointStreamingManager.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

namespace workstation { namespace pointcloud {

// ============================================================
// GPUResidencyManager - Manages GPU memory budget and LRU eviction
// ============================================================
class GPUResidencyManager {
public:
    // VRAM usage statistics
    struct Stats {
        uint64_t totalVRAMBudget;   // Maximum VRAM budget in bytes
        uint64_t vramUsedByTiles; // VRAM currently used by resident tiles
        uint64_t vramAvailable;   // Available VRAM (budget - used)
        uint32_t tilesResident;   // Number of tiles currently resident in GPU
        uint32_t tilesEvictedThisFrame;  // Tiles evicted in current frame
    };

    GPUResidencyManager();
    ~GPUResidencyManager();

    // Initialize with Vulkan device and memory budget
    bool Initialize(VkDevice device, uint64_t maxVRAMBytes = 4 * 1024 * 1024 * 1024);

    // Shutdown and clean up
    void Shutdown();

    // Register a tile as resident in GPU memory
    // Called when a tile is successfully uploaded to GPU
    void RegisterTile(uint64_t nodeKey, VkDeviceSize memoryBytes);

    // Unregister a tile when evicted from GPU memory
    void UnregisterTile(uint64_t nodeKey);

    // Mark a tile as recently used (LRU update)
    void TileUsed(uint64_t nodeKey);

    // Check if a tile is currently resident in GPU memory
    bool IsTileResident(uint64_t nodeKey) const;

    // Get current VRAM usage statistics
    Stats GetStats() const;

    // Evict least recently used tiles to stay within VRAM budget
    // Returns bytes freed by eviction
    uint64_t EvictTilesToBudget();

    // Get list of currently resident tile node keys
    std::vector<uint64_t> GetResidentTileKeys() const;

    // Get memory used by specific tile
    VkDeviceSize GetTileMemory(uint64_t nodeKey) const;

private:
    VkDevice gpuDevice_ = VK_NULL_HANDLE;
    uint64_t maxVRAMBytes_ = 4 * 1024 * 1024 * 1024;  // 4GB default budget

    // Track per-tile memory usage: nodeKey -> allocated memory size
    std::unordered_map<uint64_t, VkDeviceSize> tileMemoryMap_;

    // LRU list of resident tile keys (front = oldest = candidate for eviction)
    std::list<uint64_t> residentTilesLRU_;

    // Thread-safe statistics
    std::mutex statsMutex_;
    std::atomic<uint64_t> vramUsedByTiles_{0};
    std::atomic<uint32_t> tilesResident_{0};
    std::atomic<uint32_t> tilesEvictedThisFrame_{0};
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
// PointGPU - Point structure for GPU buffer (22 bytes packed)
// Layout per point:
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
// OctreeTile - On-disk tile representation
// ============================================================
struct OctreeTile {
    // Tile identification
    uint64_t nodeKey;          // Unique ID for GPU lookup
    BBox16 bounds;             // Bounding box for frustum culling
    uint32_t pointCount;       // Number of points in tile
    uint32_t lodLevel;         // Current LOD level (0=full, higher=simpler)
    int32_t diskOffset;        // File offset on disk (-1 if not stored)
    GPUBufState gpuState;      // Current GPU buffer state

    // Validity checksum
    uint16_t checksum;         // CRC16 of tile data

    // Constructor
    OctreeTile() : nodeKey(0), pointCount(0), lodLevel(0),
                   diskOffset(-1), gpuState(GPUBufState::NotLoaded),
                   checksum(0) {}

    // Check if tile data is valid
    bool IsValid() const {
        return pointCount > 0 &&
               gpuState != GPUBufState::NotLoaded;
    }

    // Get vertex buffer offset (22 bytes per point, interleaved XYZRGBIR)
    VkDeviceSize GetVertexOffset() const {
        return static_cast<VkDeviceSize>(pointCount) * 22;
    }

    // Get total GPU buffer size
    VkDeviceSize GetBufferSize() const {
        return GetVertexOffset();
    }

    // Get total tile size (header + point data)
    static size_t GetTileSizeBytes(uint32_t pointCount) {
        return sizeof(OctreeTile) + (22 * pointCount);
    }
};

// ============================================================
// Tile Loading Flags
// ============================================================
enum class TileLoadFlags : uint32_t {
    None        = 0x0000,
    Mipmap      = 0x0001,    // Generate mipmaps for LOD levels
    Decompress  = 0x0002,    // Decompress LAZ data
    Async       = 0x0004,    // Initiate async read (non-blocking)
    FullDecode  = 0x0008     // Fully decode all channels
};

// ============================================================
// OctreeTile Utilities
// ============================================================
class OctreeTileUtils {
public:
    // Encode tile to byte buffer for disk storage
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

// ============================================================
// Tile Chunk Header - For chunked tile storage
// ============================================================
struct TileChunkHeader {
    uint32_t magic;          // TileChunkMagic = 0x54434B48 ('TCHK')
    uint32_t version;        // Format version (current: 1)
    uint32_t chunkIndex;     // Index within tile
    uint32_t chunkPointCount; // Points in this chunk
    uint32_t chunkOffset;    // Offset within tile's point data
    uint16_t checksum;       // CRC16 of chunk data
};

constexpr uint32_t TileChunkMagic = 0x54434B48;

// ============================================================
// GPUBufState - GPU buffer state tracking
// ============================================================
enum class GPUBufState : uint8_t {
    NotLoaded = 0,      // Not yet uploaded to GPU
    CPUUploading,       // Currently being uploaded from CPU
    GPUResident,        // Successfully resident in GPU memory
    Evicted,            // Removed from GPU, can be reloaded
    MaxState
};

// ============================================================
// StreamingConfig - Configuration for streaming manager
// ============================================================
struct StreamingConfig {
    uint32_t maxGPUMemoryBytes = 4 * 1024 * 1024 * 1024;  // 4GB default
    uint32_t maxCPUMemoryBytes = 512 * 1024 * 1024;       // 512MB CPU cache
    uint32_t maxTilesResident = 500;                        // Max tiles in GPU
    uint32_t tileBudgetBytes = 256 * 1024 * 1024;         // 256MB per frame
    float tileVisibleThreshold = 0.1f;         // Min visibility fraction to keep tile
    uint32_t streamingThreadPriority = 5;                  // Thread priority
    bool enableBackgroundIO = true;                        // Async disk reads
    bool enableGPUPreload = true;                         // Preload visible tiles
};

}} // namespace workstation::pointcloud