# First Working Point Cloud Render — Integration Report

## Files Created/Modified

### New Files
| File | Purpose |
|------|---------|
| `include/workstation/renderer/PointCloudRenderAdapter.h` | Bridges PointCloud → PreparedGeometry → RenderCommand |
| `src/renderer/PointCloudRenderAdapter.cpp` | Implementation of the adapter |

### Modified Files
| File | Change |
|------|--------|
| `include/workstation/renderer/Renderer.h` | Added PointCloudRenderAdapter member, command buffer management |
| `src/renderer/Renderer.cpp` | Complete rewrite with actual Vulkan draw calls |
| `include/workstation/gpu/PreparedGeometry.h` | Added binding parameter to Bind* methods |
| `src/gpu/PreparedGeometry.cpp` | Updated Bind* methods with binding parameter |
| `app/renderer_main.cpp` | New test app with synthetic 1M point cloud |
| `CMakeLists.txt` | Added PointCloudRenderAdapter.cpp |

## Data Flow Diagram

```
PointCloud (CPU)
    │
    ▼
PointCloudRenderAdapter::PreparePointCloud()
    │
    ▼
ExtractPointCloudData() ──► PointStorage.ReadXYZ/ReadRGB
    │
    ▼
PreparedGeometry (GPU buffers via VMA)
    ├── Position Buffer  (float3 × N)
    ├── Color Buffer     (float3 × N)
    ├── Intensity Buffer (float × N)
    ├── Classification Buffer (float × N)
    └── Normal Buffer    (float3 × N)
    │
    ▼
Renderer::SetPointCloud()
    │
    ▼
Renderer::RenderFrame()
    │
    ├── vkCmdBindPipeline (RGB pipeline)
    ├── vkCmdPushConstants (camera, point size, mode)
    ├── BindPosition(cmd, 0)
    ├── BindColor(cmd, 1)
    ├── BindIntensity(cmd, 2)
    ├── BindClassification(cmd, 3)
    ├── BindNormal(cmd, 4)
    └── vkCmdDraw(N, 1, 0, 0)
    │
    ▼
Swapchain → Screen
```

## How PointCloud Connects to Vulkan

1. **PointCloud** owns a root `PointCloudNode` with `PointStorage` channels
2. **PointCloudRenderAdapter** reads XYZ/RGB/Intensity from `PointStorage` via `ReadXYZ()`/`ReadRGB()`
3. **PreparedGeometry** creates VMA-allocated VkBuffers for each attribute
4. **Renderer** binds 5 separate vertex buffers (one per attribute) and issues `vkCmdDraw`
5. Push constants provide camera matrices and visualization mode to the shader

## PointCloudRenderAdapter API

```cpp
class PointCloudRenderAdapter {
    // Prepare entire cloud (traverses root node)
    PreparedGeometry* PreparePointCloud(PointCloud& cloud);
    
    // Prepare single node
    PreparedGeometry* PrepareNode(uint64_t nodeKey, const PointCloudNode* node);
    
    // Create render command for a prepared node
    RenderCommand CreateRenderCommand(nodeKey, geometry, pipeline, layout, descriptor);
    
    // Batch create commands for visible nodes
    void CreateRenderCommandsForVisibleNodes(visibleKeys, pipeline, layout, descriptor, outCommands);
};
```

## Current Performance

- **Test dataset**: 1,000,000 points (synthetic sphere)
- **Vertex format**: Separate attribute buffers (position, color, intensity, classification, normal)
- **Draw method**: Single `vkCmdDraw` call
- **Push constants**: 128 bytes (view-projection, camera, point size, mode)

## Remaining Optimization Tasks

### Phase 2: Frustum Culling
- Wire VisibilitySystem + FrustumPlanes to cull invisible nodes
- Skip draw calls for off-screen nodes

### Phase 3: LOD + Point Budget
- Enable LODManager to select node detail levels
- Enforce ViewportPointBudget (50M point GPU budget)
- Sort commands by distance for early rejection

### Phase 4: Streaming
- Enable PointStreamingPipeline for async node loading
- Double-buffer decode + upload

### Phase 5: GPU Compute Culling
- Compile and dispatch point_culling.comp
- GPU-side frustum test + indirect draw generation

### Phase 6: Indirect Rendering
- Use vkCmdDrawIndirect for batched multi-node draws
- Single command buffer submission for entire scene
