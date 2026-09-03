cmake_minimum_required(VERSION 3.15)
project(WorkstationCAD CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# Enable OpenMP for parallel shading computation
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# ---- External Dependencies ----
# The LunarG Vulkan SDK (VULKAN_SDK) also bundles SDL3/VMA config + headers
# under the same tree, so widen the search path to find them all.
if(DEFINED ENV{VULKAN_SDK})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}")
endif()

find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)

find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    HINTS $ENV{VULKAN_SDK}/Include $ENV{VMA_INCLUDE_DIR}
    PATH_SUFFIXES vma)
if(NOT VMA_INCLUDE_DIR)
    message(FATAL_ERROR "VMA not found. Set VMA_INCLUDE_DIR.")
endif()

# ---- volk (Vulkan loader) ----
# The codebase compiles with VK_NO_PROTOTYPES (see VulkanHeaders.h), so every
# vk* entry point is resolved dynamically at runtime through volk instead of
# being link-time-bound against vulkan-1.lib. volk ships as source in the SDK.
find_path(VOLK_INCLUDE_DIR volk.h
    HINTS $ENV{VULKAN_SDK}/Include $ENV{VULKAN_SDK}/Include/Volk)
if(NOT VOLK_INCLUDE_DIR)
    message(FATAL_ERROR "volk not found (expected under the Vulkan SDK's Include/Volk).")
endif()

add_library(volk STATIC ${VOLK_INCLUDE_DIR}/volk.c)
target_include_directories(volk PUBLIC ${VOLK_INCLUDE_DIR})
target_link_libraries(volk PUBLIC Vulkan::Headers)
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)
if(WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()

# ---- Dear ImGui (vendored: no prebuilt package ships in the Vulkan SDK) ----
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# Route imgui's Vulkan backend through volk instead of linking vulkan-1.lib
# directly, matching the rest of the codebase (VK_NO_PROTOTYPES).
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(imgui PUBLIC volk SDL3::SDL3)

# ---- LASzip (LAS/LAZ point cloud file reading) ----
set(LASZIP_BUILD_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    laszip
    GIT_REPOSITORY https://github.com/LASzip/LASzip.git
    GIT_TAG        3.5.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(laszip)
# LASzip's CMake predates target-based usage requirements (no
# target_include_directories on its libraries), so wire the include paths
# manually onto the C API target we actually link against.
# NOTE: laszip_api3 (dll/laszip_api.c) is a runtime dynamic-loading shim that
# calls LoadLibrary("LASzip.dll") internally and stays a no-op stub without
# it; it is unusable when statically linked. The real static implementation
# of the same C API lives directly in laszip3 (src/laszip_dll.cpp), so link
# against that instead.
target_include_directories(laszip3 PUBLIC
    ${laszip_SOURCE_DIR}/include
    ${laszip_SOURCE_DIR}/include/laszip
    ${laszip_SOURCE_DIR}/dll
    ${laszip_BINARY_DIR}/include
)
# Upstream laszip_dll.cpp uses std::numeric_limits<uint32_t> without
# including <cstdint>; force-include it rather than patch vendored source.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(laszip3 PRIVATE -include cstdint)
endif()

# ---- Core library ----
add_library(workstation
    src/core/Element.cpp
    src/core/Model.cpp
    src/core/Document.cpp
    src/core/PointCloudEngine.cpp
    src/query/PointCloudFrustumQuery.cpp
    src/query/PointQueryExecutor.cpp
    src/spatial/SpatialTree.cpp
    src/spatial/CoordinateNormalizationManager.cpp
    src/spatial/OctreeBuilder.cpp
    src/spatial/Frustum.cpp
    src/pointcloud/PointStorage.cpp
    src/pointcloud/PointAttributeChannel.cpp
    src/pointcloud/PointCloudNode.cpp
    src/pointcloud/VoxelNode.cpp
    src/pointcloud/PointCloud.cpp
    src/pointcloud/PointCloudStreamReader.cpp
    src/pointcloud/PointBlockParser.cpp
    src/pointcloud/CloudBlockLoader.cpp
    src/pointcloud/LasFileReader.cpp
    src/pointcloud/NormalEstimator.cpp
    src/pointcloud/SntFileReader.cpp
    src/core/PointCloudScene.cpp
    src/pod/PodBinaryReader.cpp
    src/pod/PodChannelDecoder.cpp
    src/pod/PodNodeDecoder.cpp
    src/pod/PodHandlerRegistry.cpp
    src/pod/PodBlockDecoder.cpp
    src/pod/HandlerTree.cpp
    src/pod/DoubleBoundingBlockHandler.cpp
    src/pod/FileDataSource.cpp
    src/pod/PodStreamAdapter.cpp
    src/display/CachedGraphics.cpp
    src/display/GraphicsVariant.cpp
    src/display/ElementRef.cpp
    src/display/GraphicsCache.cpp
    src/display/GraphicsRecorder.cpp
    src/display/ElementGraphicsService.cpp
    src/display/RetainedGraphicsPresenter.cpp
    src/math/MathConstants.cpp
    src/math/Matrix3d.cpp
    # Vulkan backend
    src/vulkan/VmaImplementation.cpp
    src/vulkan/VulkanInstance.cpp
    src/vulkan/VulkanPhysicalDevice.cpp
    src/vulkan/VulkanDevice.cpp
    src/vulkan/VulkanSwapchain.cpp
    src/vulkan/VulkanAllocator.cpp
    src/vulkan/VulkanDescriptorManager.cpp
    src/vulkan/VulkanPipelineManager.cpp
    src/vulkan/VulkanShaderManager.cpp
    src/vulkan/VulkanRenderPass.cpp
    src/vulkan/VulkanFrameManager.cpp
    src/vulkan/VulkanCommandPool.cpp
    # Renderer layer
    src/renderer/Camera.cpp
    src/renderer/LODManager.cpp
    src/renderer/VisibilitySystem.cpp
    src/renderer/VisibilityCache.cpp
    src/renderer/PipelineCacheManager.cpp
    src/renderer/VulkanStateCache.cpp
    src/renderer/ConstantBufferManager.cpp
    src/renderer/RenderQueue.cpp
    src/renderer/ViewportPointBudget.cpp
    src/renderer/RenderContext.cpp
    src/renderer/Renderer.cpp
    src/renderer/CadRenderer.cpp
    src/renderer/PointCloudRenderAdapter.cpp
    src/renderer/ImGuiOverlay.cpp
    src/renderer/VisualizationManager.cpp
    src/renderer/DebugRenderer.cpp
    src/renderer/BenchmarkTimer.cpp
    # GPU layer
    src/gpu/GPUBufferManager.cpp
    src/gpu/PreparedGeometry.cpp
    src/gpu/GPUPointBuffer.cpp
    src/gpu/GeometryCache.cpp
    src/gpu/PointStreamingPipeline.cpp
    src/gpu/PointStreamingManager.cpp
    # Tools layer
    src/tools/ToolManager.cpp
    src/tools/SelectionTool.cpp
    src/tools/MeasurementTool.cpp
    src/tools/ClipTool.cpp
    src/tools/SectionTool.cpp
    src/tools/CrossSectionTool.cpp
    src/tools/ClassificationTool.cpp
    src/tools/CadDrawTools.cpp
    # CAD attachment system (DXF/DWG/SNT/Shading)
    src/cad/AciColorTable.cpp
    src/cad/LayerManager.cpp
    src/cad/DxfFileReader.cpp
    src/cad/DxfAttachment.cpp
    src/cad/DwgAttachment.cpp
    src/cad/SntAttachment.cpp
    src/cad/ShadingDisplay.cpp
    src/cad/AttachmentManager.cpp
    # Display mode system
    src/display/DisplayModeManager.cpp
    # Unified Scene Graph
    src/scene/SceneNode.cpp
    src/scene/SceneManager.cpp
    src/scene/PointCloudSceneObject.cpp
    src/scene/CadSceneObject.cpp
    src/scene/SceneSerializer.cpp
    # Scene Interaction System
    src/scene/RayPicker.cpp
    src/scene/SceneSelectionManager.cpp
    # Undo/Redo Command Framework
    src/core/CommandManager.cpp
    src/core/Commands.cpp
    # Overlay / Selection Rendering
    src/renderer/OverlayRenderer.cpp
    src/renderer/SelectionRenderer.cpp
    # Surface Reconstruction Pipeline
    src/surface/SurfaceLog.cpp
    src/surface/SurfaceMesh.cpp
    src/surface/SurfaceMeshGenerator.cpp
    src/surface/DelaunayTriangulator.cpp
    src/surface/AdaptiveTriangulator.cpp
    src/surface/NormalEstimator.cpp
    src/surface/SurfaceGPUBuffer.cpp
    src/surface/SurfaceRenderer.cpp
    src/surface/SurfaceMeshCache.cpp
    src/scene/SurfaceSceneObject.cpp
)

target_include_directories(workstation PUBLIC include)
target_include_directories(workstation PUBLIC
    ${Vulkan_INCLUDE_DIRS}
    ${SDL3_INCLUDE_DIRS}
    ${VMA_INCLUDE_DIR}
)
target_link_libraries(workstation PUBLIC volk SDL3::SDL3 imgui laszip3)

# ---- Tests ----
add_executable(wk_tests tests/piece1_tests.cpp)
target_link_libraries(wk_tests PRIVATE workstation)

add_executable(wk_tests2 tests/piece2_tests.cpp)
target_link_libraries(wk_tests2 PRIVATE workstation)

add_executable(wk_tests3 tests/piece3_tests.cpp)
target_link_libraries(wk_tests3 PRIVATE workstation)

add_executable(wk_tests4 tests/piece4_tests.cpp)
target_link_libraries(wk_tests4 PRIVATE workstation)

add_executable(wk_demo app/main.cpp)
target_link_libraries(wk_demo PRIVATE workstation)

enable_testing()
add_test(NAME piece1_tests COMMAND wk_tests)
add_test(NAME piece2_tests COMMAND wk_tests2)
add_test(NAME piece3_tests COMMAND wk_tests3)
add_test(NAME piece4_tests COMMAND wk_tests4)

# ---- Qt GUI ----
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(wk_gui
    app/gui_main.cpp
    src/ui/MainWindow.cpp src/ui/ViewportWidget.cpp src/ui/ModelTreeWidget.cpp
    src/ui/PropertiesWidget.cpp src/ui/ToolSettingsWidget.cpp
    src/ui/DisplayModeDialog.cpp
    src/cad/AttachmentDialogs.cpp
    include/workstation/ui/MainWindow.h include/workstation/ui/ViewportWidget.h
    include/workstation/ui/ModelTreeWidget.h include/workstation/ui/PropertiesWidget.h
    include/workstation/ui/ToolSettingsWidget.h
    include/workstation/ui/DisplayModeDialog.h
    include/workstation/cad/AttachmentDialogs.h
    include/workstation/display/DisplayModeManager.h
)
target_link_libraries(wk_gui PRIVATE workstation Qt6::Widgets)
target_include_directories(wk_gui PRIVATE include)

# ---- Piece tests ----
add_executable(wk_pc_tests tests/pointcloud_query_tests.cpp)
target_link_libraries(wk_pc_tests PRIVATE workstation)
add_test(NAME piece_pc COMMAND wk_pc_tests)

add_executable(wk_pc2_tests tests/pointcloud_scene_tests.cpp)
target_link_libraries(wk_pc2_tests PRIVATE workstation)
add_test(NAME piece_pc2 COMMAND wk_pc2_tests)

add_executable(wk_pc3_tests tests/pointcloud_block_stream_tests.cpp)
target_link_libraries(wk_pc3_tests PRIVATE workstation)
add_test(NAME piece_pc3 COMMAND wk_pc3_tests)

add_executable(wk_pod_tests tests/pod_block_decoder_tests.cpp)
target_link_libraries(wk_pod_tests PRIVATE workstation)
add_test(NAME piece_pod COMMAND wk_pod_tests)

add_executable(wk_pod_handler_tests tests/pod_handler_registry_tests.cpp)
target_link_libraries(wk_pod_handler_tests PRIVATE workstation)
add_test(NAME piece_pod_handler COMMAND wk_pod_handler_tests)

add_executable(wk_pod_datasource_tests tests/pod_datasource_tests.cpp)
target_link_libraries(wk_pod_datasource_tests PRIVATE workstation)
add_test(NAME piece_pod_datasource COMMAND wk_pod_datasource_tests)

add_executable(wk_pod_integration_tests tests/pod_integration_tests.cpp)
target_link_libraries(wk_pod_integration_tests PRIVATE workstation)
add_test(NAME piece_pod_integration COMMAND wk_pod_integration_tests)

add_executable(wk_surface_tests tests/surface_pipeline_tests.cpp)
target_link_libraries(wk_surface_tests PRIVATE workstation)
add_test(NAME piece_surface COMMAND wk_surface_tests)

# ---- Vulkan Renderer ----
add_executable(wk_renderer app/renderer_main.cpp)
target_link_libraries(wk_renderer PRIVATE workstation)
target_include_directories(wk_renderer PRIVATE include)

# Auto-compile GLSL shaders to SPIR-V when source changes
find_program(GLSLC glslc HINTS "H:/Tools/Bin")
if(GLSLC)
    set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
    set(SHADER_DST_DIR ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_DST_DIR})
    foreach(SHADER point.vert point.frag line.vert line.frag debug_point.vert debug_point.frag point_classification.frag point_height.frag point_intensity.frag point_normal.frag cad_line.vert cad_line.frag cad_point.vert cad_point.frag cad_surface.vert cad_surface.frag picking.vert picking.frag selection.vert selection.frag surface.vert surface.frag)
        add_custom_command(
            OUTPUT ${SHADER_DST_DIR}/${SHADER}.spv
            COMMAND ${GLSLC} ${SHADER_SRC_DIR}/${SHADER} -o ${SHADER_DST_DIR}/${SHADER}.spv
            DEPENDS ${SHADER_SRC_DIR}/${SHADER}
            COMMENT "Compiling ${SHADER} -> ${SHADER}.spv"
        )
    endforeach()
    add_custom_target(shaders ALL DEPENDS
        ${SHADER_DST_DIR}/point.vert.spv ${SHADER_DST_DIR}/point.frag.spv
        ${SHADER_DST_DIR}/line.vert.spv ${SHADER_DST_DIR}/line.frag.spv
        ${SHADER_DST_DIR}/cad_line.vert.spv ${SHADER_DST_DIR}/cad_line.frag.spv
        ${SHADER_DST_DIR}/cad_point.vert.spv ${SHADER_DST_DIR}/cad_point.frag.spv
        ${SHADER_DST_DIR}/cad_surface.vert.spv ${SHADER_DST_DIR}/cad_surface.frag.spv
        ${SHADER_DST_DIR}/picking.vert.spv ${SHADER_DST_DIR}/picking.frag.spv
        ${SHADER_DST_DIR}/selection.vert.spv ${SHADER_DST_DIR}/selection.frag.spv
        ${SHADER_DST_DIR}/surface.vert.spv ${SHADER_DST_DIR}/surface.frag.spv)
    add_dependencies(wk_renderer shaders)
    # wk_gui reads its shaders from the same CMAKE_BINARY_DIR/shaders/ output
    # (see Renderer::CreatePointPipeline's GetExeShaderDir()) -- without this,
    # `ninja wk_gui` (as opposed to a full/default build) silently skips
    # shader recompilation since ALL only applies to the default target.
    add_dependencies(wk_gui shaders)
else()
    message(WARNING "glslc not found - shaders will not be auto-compiled")
endif()
