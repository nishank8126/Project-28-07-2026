cmake_minimum_required(VERSION 3.15)
project(WorkstationCAD CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# ---- External Dependencies ----
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)

find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    HINTS $ENV{VULKAN_SDK}/Include $ENV{VMA_INCLUDE_DIR})
if(NOT VMA_INCLUDE_DIR)
    message(FATAL_ERROR "VMA not found. Set VMA_INCLUDE_DIR.")
endif()

find_path(IMGUI_INCLUDE_DIR imgui.h
    HINTS $ENV{IMGUI_INCLUDE_DIR})
if(NOT IMGUI_INCLUDE_DIR)
    message(FATAL_ERROR "ImGui not found. Set IMGUI_INCLUDE_DIR.")
endif()

find_path(IMGUI_IMPL_DIR imgui_impl_vulkan.h
    HINTS ${IMGUI_INCLUDE_DIR}/backends $ENV{IMGUI_IMPL_DIR})

# ---- Core library ----
add_library(workstation
    src/core/Element.cpp
    src/core/Model.cpp
    src/core/Document.cpp
    src/core/PointCloudEngine.cpp
    src/query/PointCloudFrustumQuery.cpp
    src/query/PointQueryExecutor.cpp
    src/spatial/SpatialTree.cpp
    src/pointcloud/PointStorage.cpp
    src/pointcloud/PointAttributeChannel.cpp
    src/pointcloud/PointCloudNode.cpp
    src/pointcloud/VoxelNode.cpp
    src/pointcloud/PointCloud.cpp
    src/pointcloud/PointCloudStreamReader.cpp
    src/pointcloud/PointBlockParser.cpp
    src/pointcloud/CloudBlockLoader.cpp
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
    src/renderer/PointCloudRenderAdapter.cpp
    src/renderer/ImGuiOverlay.cpp
    # GPU layer
    src/gpu/GPUBufferManager.cpp
    src/gpu/PreparedGeometry.cpp
    src/gpu/GPUPointBuffer.cpp
    src/gpu/GeometryCache.cpp
    src/gpu/PointStreamingPipeline.cpp
    src/gpu/PointStreamingManager.cpp
)

target_include_directories(workstation PUBLIC include)
target_include_directories(workstation PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${SDL3_INCLUDE_DIRS}
    ${VMA_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}
    ${IMGUI_IMPL_DIR}
)
target_link_libraries(workstation PUBLIC ${Vulkan_LIBRARIES} SDL3::SDL3)

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
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(wk_gui
    app/gui_main.cpp
    src/ui/MainWindow.cpp src/ui/ViewportWidget.cpp src/ui/ModelTreeWidget.cpp
    src/ui/PropertiesWidget.cpp src/ui/ToolSettingsWidget.cpp
    include/workstation/ui/MainWindow.h include/workstation/ui/ViewportWidget.h
    include/workstation/ui/ModelTreeWidget.h include/workstation/ui/PropertiesWidget.h
    include/workstation/ui/ToolSettingsWidget.h
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

# ---- Vulkan Renderer ----
add_executable(wk_renderer app/renderer_main.cpp)
target_link_libraries(wk_renderer PRIVATE workstation)
target_include_directories(wk_renderer PRIVATE include)
