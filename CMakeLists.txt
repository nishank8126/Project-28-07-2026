cmake_minimum_required(VERSION 3.15)
project(WorkstationCAD CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# ---- Core library (no Direct3D / GUI dependency) ----
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
)

target_include_directories(workstation PUBLIC include)

# ---- Tests ----
add_executable(wk_tests tests/piece1_tests.cpp)
target_link_libraries(wk_tests PRIVATE workstation)

add_executable(wk_tests2 tests/piece2_tests.cpp)
target_link_libraries(wk_tests2 PRIVATE workstation)

add_executable(wk_tests3 tests/piece3_tests.cpp)
target_link_libraries(wk_tests3 PRIVATE workstation)

# ---- Demo ----
add_executable(wk_demo app/main.cpp)
target_link_libraries(wk_demo PRIVATE workstation)

add_executable(wk_tests4 tests/piece4_tests.cpp)
target_link_libraries(wk_tests4 PRIVATE workstation)

enable_testing()
add_test(NAME piece1_tests COMMAND wk_tests)
add_test(NAME piece2_tests COMMAND wk_tests2)
add_test(NAME piece3_tests COMMAND wk_tests3)
add_test(NAME piece4_tests COMMAND wk_tests4)
add_test(NAME piece_pc COMMAND wk_pc_tests)

# ---- GUI Shell 1 (Qt 6 Widgets) ----
# Pure implementation mechanics: window, docks, menus, toolbar, status bar,
# native HWND-capable viewport placeholder. No CAD/renderer/camera algorithms.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(wk_gui
    app/gui_main.cpp
    src/ui/MainWindow.cpp
    src/ui/ViewportWidget.cpp
    src/ui/ModelTreeWidget.cpp
    src/ui/PropertiesWidget.cpp
    src/ui/ToolSettingsWidget.cpp
    include/workstation/ui/MainWindow.h
    include/workstation/ui/ViewportWidget.h
    include/workstation/ui/ModelTreeWidget.h
    include/workstation/ui/PropertiesWidget.h
    include/workstation/ui/ToolSettingsWidget.h
)
target_link_libraries(wk_gui PRIVATE workstation Qt6::Widgets)
target_include_directories(wk_gui PRIVATE include)
if(WIN32)
    set_target_properties(wk_gui PROPERTIES WIN32_EXECUTABLE ON)
endif()

# ---- GUI Shell 1 smoke test (validation harness, not a product feature) ----
add_executable(wk_gui_smoke
    tests/gui_smoke.cpp
    src/ui/MainWindow.cpp
    src/ui/ViewportWidget.cpp
    src/ui/ModelTreeWidget.cpp
    src/ui/PropertiesWidget.cpp
    src/ui/ToolSettingsWidget.cpp
    include/workstation/ui/MainWindow.h
    include/workstation/ui/ViewportWidget.h
    include/workstation/ui/ModelTreeWidget.h
    include/workstation/ui/PropertiesWidget.h
    include/workstation/ui/ToolSettingsWidget.h
)
target_link_libraries(wk_gui_smoke PRIVATE workstation Qt6::Widgets)
target_include_directories(wk_gui_smoke PRIVATE include)

# ---- Piece 1: Point Cloud Query Foundation tests ----
add_executable(wk_pc_tests tests/pointcloud_query_tests.cpp)
target_link_libraries(wk_pc_tests PRIVATE workstation)

# ---- Piece 2: Scene/Cloud/Node/Voxel/Channel/Storage tests ----
add_executable(wk_pc2_tests tests/pointcloud_scene_tests.cpp)
target_link_libraries(wk_pc2_tests PRIVATE workstation)
add_test(NAME piece_pc2 COMMAND wk_pc2_tests)

# ---- Piece 3: Block streaming & scene population tests ----
add_executable(wk_pc3_tests tests/pointcloud_block_stream_tests.cpp)
target_link_libraries(wk_pc3_tests PRIVATE workstation)
add_test(NAME piece_pc3 COMMAND wk_pc3_tests)

# ---- Piece 4: POD Block Decoder tests ----
add_executable(wk_pod_tests tests/pod_block_decoder_tests.cpp)
target_link_libraries(wk_pod_tests PRIVATE workstation)
add_test(NAME piece_pod COMMAND wk_pod_tests)

# ---- Piece 5: Handler Registry tests ----
add_executable(wk_pod_handler_tests tests/pod_handler_registry_tests.cpp)
target_link_libraries(wk_pod_handler_tests PRIVATE workstation)
add_test(NAME piece_pod_handler COMMAND wk_pod_handler_tests)

# ---- Piece 5: DataSource tests ----
add_executable(wk_pod_datasource_tests tests/pod_datasource_tests.cpp)
target_link_libraries(wk_pod_datasource_tests PRIVATE workstation)
add_test(NAME piece_pod_datasource COMMAND wk_pod_datasource_tests)

# ---- Piece 5: Integration tests ----
add_executable(wk_pod_integration_tests tests/pod_integration_tests.cpp)
target_link_libraries(wk_pod_integration_tests PRIVATE workstation)
add_test(NAME piece_pod_integration COMMAND wk_pod_integration_tests)
