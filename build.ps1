# DiverseShot CMake Build Script
# 支持 Debug, Release, Production 三种构建配置

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release", "Production")]
    [string]$Config = "Release",
    
    [Parameter(Mandatory=$false)]
    [switch]$Clean,
    
    [Parameter(Mandatory=$false)]
    [int]$Jobs = 8
)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " DiverseShot Build Script" -ForegroundColor Cyan
Write-Host " Configuration: $Config" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Check environment variables
Write-Host "`nChecking environment variables..." -ForegroundColor Yellow
if (-not $env:PY3_PATH) {
    Write-Host "Warning: PY3_PATH environment variable not set!" -ForegroundColor Red
}
if (-not $env:VK_SDK_PATH -and -not $env:VULKAN_SDK) {
    Write-Host "Warning: VK_SDK_PATH or VULKAN_SDK environment variable not set!" -ForegroundColor Red
}
if (-not $env:CUDA_PATH) {
    Write-Host "Warning: CUDA_PATH environment variable not set!" -ForegroundColor Red
}

Write-Host "  PY3_PATH: $env:PY3_PATH" -ForegroundColor Cyan
Write-Host "  VK_SDK_PATH: $env:VK_SDK_PATH" -ForegroundColor Cyan
Write-Host "  VULKAN_SDK: $env:VULKAN_SDK" -ForegroundColor Cyan
Write-Host "  CUDA_PATH: $env:CUDA_PATH" -ForegroundColor Cyan

# Check for vcpkg toolchain file
$vcpkgToolchain = ""
if ($env:VCPKG_ROOT) {
    $vcpkgToolchain = "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
    if (Test-Path $vcpkgToolchain) {
        Write-Host "  VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Cyan
        Write-Host "  Using vcpkg toolchain: $vcpkgToolchain" -ForegroundColor Green
    } else {
        $vcpkgToolchain = ""
    }
} elseif ($env:CMAKE_TOOLCHAIN_FILE) {
    $vcpkgToolchain = $env:CMAKE_TOOLCHAIN_FILE
    Write-Host "  Using CMAKE_TOOLCHAIN_FILE: $vcpkgToolchain" -ForegroundColor Green
} else {
    # Try common vcpkg locations
    $commonVcpkgPaths = @(
        "C:\vcpkg\scripts\buildsystems\vcpkg.cmake",
        "C:\tools\vcpkg\scripts\buildsystems\vcpkg.cmake"
    )
    foreach ($path in $commonVcpkgPaths) {
        if (Test-Path $path) {
            $vcpkgToolchain = $path
            Write-Host "  Found vcpkg toolchain: $vcpkgToolchain" -ForegroundColor Green
            break
        }
    }
}

# Clean build directory if requested
if ($Clean) {
    Write-Host "`nCleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Configure CMake
Write-Host "`nConfiguring CMake..." -ForegroundColor Green
$cmakeArgs = @("-B", "build", "-Wno-dev", "-DCMAKE_BUILD_TYPE=$Config")
if ($vcpkgToolchain) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
}
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "`nBuilding with configuration: $Config" -ForegroundColor Green
cmake --build build --config $Config -j $Jobs
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuild completed successfully!" -ForegroundColor Green
$configLower = $Config.ToLower()
Write-Host "Binary location: build/$configLower/" -ForegroundColor Cyan



