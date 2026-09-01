# NakshaPointEngine — Vulkan Rendering Architecture

## 1. Module Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                         │
│  renderer_main.cpp  ─────────────────────────────────────────►  │
│       │                                                         │
│       ▼                                                         │
├─────────────────────────────────────────────────────────────────┤
│                    workstation::renderer                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │ Renderer │ │ Camera   │ │ LOD      │ │ VisibilitySystem │   │
│  │          │ │ System   │ │ Manager  │ │                  │   │
│  └────┬─────┘ └──────────┘ └──────────┘ └──────────────────┘   │
│       │                                                         │
│  ┌────┴─────┐ ┌──────────────┐ ┌──────────────────────────┐    │
│  │ Render   │ │ Render       │ │ ImGuiOverlay             │    │
│  │ Context  │ │ Queue        │ │ (Debug panels)           │    │
│  └──────────┘ └──────────────┘ └──────────────────────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                    workstation::vulkan                            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │ Vulkan   │ │ Vulkan   │ │ Vulkan   │ │ VulkanSwapchain  │   │
│  │ Instance │ │ Device   │ │ Physical │ │                  │   │
│  │          │ │          │ │ Device   │ │                  │   │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │ Vulkan   │ │ Vulkan   │ │ Vulkan   │ │ VulkanPipeline   │   │
│  │ Allocator│ │ Descriptor│ │ Shader  │ │ Manager          │   │
│  │ (VMA)    │ │ Manager  │ │ Manager  │ │                  │   │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘   │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────────────────┐    │
│  │ Vulkan   │ │ Vulkan   │ │ VulkanFrameManager           │    │
│  │ Render   │ │ Command  │ │ (Sync objects, frame state)  │    │
│  │ Pass     │ │ Pool     │ │                              │    │
│  └──────────┘ └──────────┘ └──────────────────────────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                    workstation::gpu                               │
│  ┌──────────────┐ ┌──────────────────┐ ┌──────────────────┐    │
│  │ GPUPointBuffer│ │ GPUResourceManager│ │ PointStreaming  │    │
│  │ (VBO wrapper)│ │ (Node buffer pool)│ │ Pipeline        │    │
│  └──────────────┘ └──────────────────┘ └──────────────────┘    │
├─────────────────────────────────────────────────────────────────┤
│                    workstation::pointcloud (existing)             │
│  PointCloud  ──►  PointCloudNode  ──►  PointStorage             │
│                                            │                    │
│                                     PointAttributeChannel       │
└─────────────────────────────────────────────────────────────────┘
```

## 2. Data Flow

```
Disk (.pod/.laz)
    │
    ▼
PointCloudStreamReader (256KB buffer)
    │
    ▼
PodBlockDecoder
    │
    ▼
PointCloud (CPU memory)
    │
    ▼
PointStreamingPipeline (decode thread)
    │
    ▼
GPUResourceManager (VMA allocation)
    │
    ▼
GPUPointBuffer (Vulkan VBO)
    │
    ▼
RenderQueue → Command Buffer → Vulkan Draw → Screen
```

## 3. Vulkan Backend Components

### VulkanInstance
- Creates VkInstance with validation layers
- Enumerates required SDL3 Vulkan extensions
- Debug messenger for validation messages

### VulkanPhysicalDevice
- Scores and selects best GPU (discrete preferred)
- Finds queue families (graphics, present, compute, transfer)
- Queries swapchain support capabilities

### VulkanDevice
- Creates logical device with required queues
- Enables device features (anisotropy, wide lines)

### VulkanSwapchain
- Manages swapchain lifecycle (create, recreate, destroy)
- Creates color and depth image views
- Acquires/presents images

### VulkanAllocator (VMA)
- GPU buffer allocation with VMA
- Staging buffer support for CPU→GPU uploads
- Memory statistics tracking

### VulkanDescriptorManager
- Creates descriptor set layouts
- Allocates descriptor sets
- Updates buffer/image descriptors

### VulkanPipelineManager
- Creates graphics pipelines (point list topology)
- Creates compute pipelines (future GPU culling)
- Manages pipeline layouts with push constants

### VulkanShaderManager
- Loads SPIR-V files from disk
- Creates VkShaderModule objects
- Compiles shader variants

### VulkanRenderPass
- Color + depth attachment configuration
- Subpass dependencies for correct synchronization

### VulkanFrameManager
- Manages frames-in-flight (double buffering)
- Fence/semaphore synchronization

## 4. GPU Point Cloud System

### GPUPointBuffer
- Wraps a Vulkan VBO created through VMA
- Supports Static, Dynamic, Streaming usage patterns
- Upload with memcpy for coherent memory
- Bind and draw commands

### PointVertex Layout (48 bytes)
```
Offset  Size  Field
0       12    position (float[3])
12      12    color (float[3])
24      4     intensity (float)
28      4     classification (float)
32      12    normal (float[3])
44      8     padding (float[2])
```

### GPUResourceManager
- LRU cache for node GPU buffers
- Per-node buffer allocation
- Eviction of unused buffers (frame-based age)

### PointStreamingPipeline
- Background decode thread
- Priority queue for upload requests
- CPU→GPU transfer in batches
- Cancellation support

## 5. Camera System

### Camera
- Perspective and orthographic projection
- Look-at positioning
- WASD + mouse movement (forward/right/pan/rotate/zoom)
- Frustum plane extraction from view-projection matrix

### FrustumPlanes
- 6-plane frustum (left, right, top, bottom, near, far)
- AABB-vs-frustum test for visibility culling
- Normalized plane equations

## 6. Visibility System

### VisibilitySystem
- Traverses SpatialTree with frustum test
- Integrates LOD budget from LODManager
- Reports visible nodes and culled nodes

## 7. LOD System

### LODManager
- Screen-space error calculation per node
- Point budget enforcement
- Priority-sorted visible node list
- Progressive loading support

### LODConfig
```
totalPointBudget:      10,000,000
visiblePointBudget:     5,000,000
minScreenSpaceError:    1.0
maxScreenSpaceError:    100.0
```

## 8. Shaders

### point.vert
- Push constants: viewProjection, cameraPosition, pointScale, visualizationMode
- SSBO input: point data (position, color, intensity, classification, normal)
- 7 visualization modes: RGB, Intensity, Classification, Elevation, HeightRamp, NormalShading, Density
- Point size attenuation by distance

### point.frag
- Circular point shape (discard outside radius)
- Edge fade for smooth appearance
- Depth-based alpha fade

### point_culling.comp
- GPU frustum culling compute shader
- Shared memory atomic counter for output
- Indirect draw buffer generation

## 9. Debug System (ImGui)

### Panels
- **Renderer Debug**: FPS, frame time, visible points, draw calls
- **LOD**: Point budget usage, LOD level, point size slider
- **GPU**: VMA memory stats, buffer count, image count
- **Visualization**: Mode selector, bounding box toggle, frustum culling toggle

## 10. External Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| Vulkan SDK | 1.3+ | Graphics API |
| VMA | Latest | GPU memory allocation |
| SDL3 | Latest | Window management, input, Vulkan surface |
| ImGui | Latest | Debug overlay UI |
| glslang | Bundled | Shader compilation (glslc) |

## 11. Build Instructions

```bash
# Set environment
set PATH=H:\Tools\mingw1310_64\bin;H:\Tools\Ninja;%PATH%

# Configure
cmake -G Ninja -B build-gui -DCMAKE_PREFIX_PATH="H:/6.11.2/mingw_64" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-gui --target wk_renderer

# Compile shaders (requires glslc in PATH)
cd shaders && compile_shaders.bat
```

## 12. Clean-Room Compliance

All names, types, and interfaces in the renderer are independently designed:
- `Renderer`, `RenderContext`, `Camera`, `LODManager`, `VisibilitySystem` — generic names
- `GPUPointBuffer`, `GPUResourceManager`, `PointStreamingPipeline` — descriptive names
- `VulkanInstance`, `VulkanDevice`, `VulkanSwapchain` — standard Vulkan naming
- No proprietary offsets, ABI, or implementation details are used
- Rendering concepts derived from public Vulkan tutorials and API documentation
