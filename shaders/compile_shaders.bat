@echo off
where glslc >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set GLSLC=glslc
) else if exist "%VULKAN_SDK%\Bin\glslc.exe" (
    set GLSLC="%VULKAN_SDK%\Bin\glslc.exe"
) else (
    echo glslc not found on PATH and VULKAN_SDK is not set. Install/select the Vulkan SDK.
    exit /b 1
)
set SHADER_DIR=%~dp0
set OUTPUT_DIR=%~dp0../build-gui/shaders

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo Compiling shaders...

%GLSLC% "%SHADER_DIR%point.vert" -o "%OUTPUT_DIR%/point.vert.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point.vert & exit /b 1)
echo   point.vert -^> point.vert.spv

%GLSLC% "%SHADER_DIR%point.frag" -o "%OUTPUT_DIR%/point.frag.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point.frag & exit /b 1)
echo   point.frag -^> point.frag.spv

%GLSLC% "%SHADER_DIR%point_intensity.frag" -o "%OUTPUT_DIR%/point_intensity.frag.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point_intensity.frag & exit /b 1)
echo   point_intensity.frag -^> point_intensity.frag.spv

%GLSLC% "%SHADER_DIR%point_classification.frag" -o "%OUTPUT_DIR%/point_classification.frag.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point_classification.frag & exit /b 1)
echo   point_classification.frag -^> point_classification.frag.spv

%GLSLC% "%SHADER_DIR%point_height.frag" -o "%OUTPUT_DIR%/point_height.frag.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point_height.frag & exit /b 1)
echo   point_height.frag -^> point_height.frag.spv

%GLSLC% "%SHADER_DIR%point_normal.frag" -o "%OUTPUT_DIR%/point_normal.frag.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point_normal.frag & exit /b 1)
echo   point_normal.frag -^> point_normal.frag.spv

%GLSLC% "%SHADER_DIR%point_culling.comp" -o "%OUTPUT_DIR%/point_culling.comp.spv" --target-env=vulkan1.3
if %ERRORLEVEL% neq 0 (echo FAILED: point_culling.comp & exit /b 1)
echo   point_culling.comp -^> point_culling.comp.spv

echo All shaders compiled successfully.
