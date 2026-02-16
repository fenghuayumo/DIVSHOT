<div align="center">

# DIVSHOT 

**Official Website:** [https://divshot.app/](https://divshot.app/)
</div>

### Introduction

**A lightweight radiance field renderer and training platform client that supports flexible editing tools for editing and cropping trained Gaussian splats for export, as well as rendering them to images or videos.**

It originated from a 2023 paper [gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting). Initially, it was created for learning purposes to implement this paper in C++, aiming to build a real-time visualization training tool similar to [instantngp](https://github.com/NVlabs/instant-ngp). After the emergence of SuperSplat, I was inspired to expand its functionality. After 2 years of spare time development, it has now become a comprehensive and practical tool.

</div>

## Screenshots
![DIVSHOT](/screenshots/screenshot.png?raw=true)
![DIVSHOT](/screenshots/screenshot2.png?raw=true)
![DIVSHOT](/screenshots/screenshot3.png?raw=true)
splat -->  mesh --> texturemesh
![DIVSHOT](/screenshots/gs_mesh.png?raw=true)
<!-- ## 🎥 Videos
![DIVSHOT](/screenshots/video.mp4?) -->

## User Guide
- [Official Website](https://divshot.app/)
- [UserGuide][UserGuide] user documentation
## Video Examples
- [DIVSHOT Demo on Bilibili](https://www.bilibili.com/video/BV1KEsZzXEPd/?spm_id_from=333.1387.homepage.video_card.click)
- [DIVSHOT Demo on Youtube](https://www.youtube.com/watch?v=MJsJNsayIco)

- [Online Docs](https://divshot.app/docs.html)

<!-- * 4. Python3 -->
**Note:** Please ensure you update to the latest graphics card driver, otherwise there may be some issues.

**Explanation:** The current program consists of 2 parts: the training part and the rendering/editing part. These two parts are relatively independent and decoupled. Currently, the open-source code includes the rendering and editing part, which is written using Vulkan and C++, meaning it can be compiled and run on multiple platforms. The training part is implemented using libtorch C++ and CUDA, which requires running on NVIDIA graphics cards. Although CUDA can be run on AMD graphics cards through the AMD HIP technology framework, this requires additional development time and will be completed in the future.

Operating systems:

* Windows
* Mac (Will be in future)
* Linux (Will be in future)

**Note:** Currently refactoring a new framework to support multi-platform training, planned to be named RadField Studio. It will become a professional scanning tool for CG workflows. Stay tuned!

## Run

For general users, you can directly use the latest [Release](https://github.com/fenghuayumo/DIVSHOT/releases) installation package, which includes all dependencies and can be run directly.

Note: The training source code does not provide SFM calculation for camera poses and point clouds. Please use third-party software to process and obtain them, then drag the corresponding files to the viewport. Currently supports Colmap, RealityCapture, NerfStudio, MetaShape.
## Project structure
<!-- 
*  diverse
   *  the core rendering lib
*  diverseshot
   *  the editor exe
   *   
* -->
|Directory                                  |Details                                                                    |
|-------------------------------------------|---------------------------------------------                              |
|[/diverse][diverse]                        |the core rendering lib including render lib an other useful function       |
|[/application/editor][editor]                |the editor exe, main  program                                       
|[/application/diverseshot-cli][diverseshot-cli]                |gsplat training console exe             |
|[/diverse_utils/gstrain][gstrain]                        |the core gsplat training lib which use gsplatrast lib to forward and backward render, contains data loader process                                   |
|[/external][external]                      |external lib                            |

### Build
#### Windows
you should install compile build tools and package firstly
1. VS2022
2. Cuda SDK (Cuda 12.8)
3. Vulkan SDK
4. CMake
5. Python

Set PY3_PATH="Python3 install directory"  and VCPKG_ROOT="vcpkg.exe directory" enviroment variable
and ADD "D:\XXXX\Pyton3\Lib\site-packages\torch\lib" to PATH enviroment variable
## Building and running
```
git clone https://github.com/fenghuayumo/DIVSHOT.git
git submodule update --init --recursive

$env:VCPKG_ROOT\vcpkg.exe install opencv[core,contrib,ffmpeg]:x64-windows
$env:VCPKG_ROOT\vcpkg.exe install \
    glm:x64-windows \
    tinyply:x64-windows \
    libarchive:x64-windows \
    webp:x64-windows \
    zlib:x64-windows \
    nlohmann-json:x64-windows \
    tl-expected:x64-windows

pip3 install torch==2.7.1 torchvision==0.22.1 torchaudio==2.7.1 --index-url https://download.pytorch.org/whl/cu128

$env:PY3_PATH         # Python PATH
$env:CUDA_PATH        # CUDA PATH  
$env:VK_SDK_PATH      # Vulkan SDK PATH
$env:VCPKG_ROOT       # vcpkg PATH: D:\XXXX\vcpkg）
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## Problem

If you have any questions or ideas about 3D reconstruction and graphics, feel free to join the following communities. If you speak Chinese, you can join the QQ group or WeChat group below.

| **QQ Group** | **WeChat Group** |
|:---:|:---:|
| ![QQ Group](/screenshots/qq_group.jpg?raw=true) | ![WeChat Group](/screenshots/wecaht_gropu.jpg?raw=true) |

# Credits

 * [imgui](https://github.com/ocornut/imgui) : Dear ImGui: Bloat-free Immediate Mode Graphical User interface for C++ with minimal dependencies.
 * [imguizmo](https://github.com/CedricGuillemet/ImGuizmo) : Immediate mode 3D gizmo for scene editing and other controls based on Dear Imgui.
 * [entt](https://github.com/skypjack/entt) : Fast and reliable entity-component system (ECS) 
 * [glfw](https://github.com/glfw/glfw) : A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input.
 * [spdlog](https://github.com/gabime/spdlog) : Fast C++ logging library.
 * [stb](https://github.com/nothings/stb) : Single-file public domain (or MIT licensed) libraries for C/C++.
 * [tinygltf](https://github.com/syoyo/tinygltf) : Header only C++11 tiny glTF 2.0 library
 * [tinyobjloader](https://github.com/syoyo/tinyobjloader) : Tiny but powerful single file wavefront obj loader
 * [volk](https://github.com/zeux/volk) : Meta loader for Vulkan API.
 * [glad](https://github.com/Dav1dde/glad) : Meta loader for OpenGL API.
 * [cereal](https://github.com/USCiLab/cereal) : A C++11 library for serialization
 * [meshoptimizer](https://github.com/zeux/meshoptimizer) : Mesh optimization library that makes meshes smaller and faster to render
 * [4d gaussians](https://github.com/hustvl/4DGaussians)
 * [deformable-3d-gaussians](https://github.com/ingra14m/Deformable-3D-Gaussians)
 * [diff-gaussian-rasterization](https://github.com/graphdeco-inria/diff-gaussian-rasterization)
 * [dynamic-3d-gaussians](https://github.com/JonathonLuiten/Dynamic3DGaussians)
 * [ewa splatting](https://www.cs.umd.edu/~zwicker/publications/EWASplatting-TVCG02.pdf) 
 * [gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting)
 * [making gaussian splats smaller](https://aras-p.info/blog/2023/09/13/Making-Gaussian-Splats-smaller/)
 * [spacetime-gaussians](https://github.com/oppo-us-research/SpacetimeGaussians)
 * [sugar](https://github.com/Anttwo/SuGaR)
 * [rain-gs](https://ku-cvlab.github.io/RAIN-GS/)
 * [eagles](https://github.com/Sharath-girish/efficientgaussian)
 * [pixelgs](https://arxiv.org/abs/2403.15530)
 * [opgs](https://arxiv.org/html/2402.00752v2)
 * [lightgs](https://github.dev/VITA-Group/LightGaussian/blob/main/)
 * [vastgs](https://vastgaussian.github.io/)
 * [octgs](https://github.com/city-super/Octree-GS/tree/main)
 * [sa-gs](https://github.com/zsy1987/SA-GS/tree/master)
 * [BAGS](https://nwang43jhu.github.io/BAGS/)
 * [4dgs](https://github.com/fudan-zvg/4d-gaussian-splatting)
 * [instantngp](https://github.com/NVlabs/instant-ngp)

[UserGuide]: docs/userGuide.md    
[DevGuide]:  docs/devGuide.md
[docs]: docs
[diverse]: diverse
[external]: external
[editor]: application/editor
[diverseshot-cli]: application/diverseshot-cli
[gstrain]: diverse_utils/gstrain/