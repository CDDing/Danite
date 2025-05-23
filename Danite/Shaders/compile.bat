@echo off
setlocal enabledelayedexpansion

:: Vulkan SDK glslc 경로 설정
set GLSLC_PATH=C:\VulkanSDK\1.4.304.0\Bin\glslc.exe

:: 현재 디렉토리에서 모든 .vert와 .frag 파일 처리
for %%f in (*.vert *.frag) do (
    echo Compiling %%f to %%f.spv...
    "%GLSLC_PATH%" %%f -o %%f.spv
)
for %%f in (*.rgen *.rchit *.rmiss) do (
    echo Compiling %%f to %%f.spv with SPIR-V 1.5...
    "%GLSLC_PATH%" %%f -o %%f.spv --target-spv=spv1.5
)
echo All shaders compiled successfully.
pause