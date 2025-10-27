<div align="center">

# DIVSHOT 
**a lightweight radiance field renderer and trainng platform client**

</div>

## Screenshots
![DIVSHOT](/screenshots/screenshot.png?raw=true)
![DIVSHOT](/screenshots/screenshot2.png?raw=true)
![DIVSHOT](/screenshots/screenshot3.png?raw=true)
<!-- ## 🎥 Videos
![DIVSHOT](/screenshots/video.mp4?) -->

## User Guide
- [UserGuide][UserGuide] user documentation
## Video Examples
- [DIVSHOT Demo on Bilibili](https://www.bilibili.com/video/BV1KEsZzXEPd/?spm_id_from=333.1387.homepage.video_card.click)
   
## Install On Windows
* 1. VS2022
* 2. Cuda SDK
* 3. Vulkan SDK
* 4. Python3

Operating systems:

* Windows
* Mac (Will be in future)
* Linux (Will be in future)

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
|[/application/editor][diverseshot]                |the editor exe, main  program                                       
|[/application/diverseshot-cli][diverseshot-cli]                |gsplat training console exe             |
|[/external][external]                      |external lib                            |
## Building and running
```
git clone https://github.com/fenghuayumo/DIVSHOT.git
git submodule update --init --recursive
```

To build `divshot` you should open the scripts folder, Then, doing the following operations

#### Windows 
* Run Scripts/GenerateVS.bat to generate a visual studio project, you can select divshot as startup project.


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
 * [phys-gaussian](https://xpandora.github.io/PhysGaussian/)
 * [scaffold-gs](https://city-super.github.io/scaffold-gs/)
 * [spacetime-gaussians](https://github.com/oppo-us-research/SpacetimeGaussians)
 * [sugar](https://github.com/Anttwo/SuGaR)
 * [rain-gs](https://ku-cvlab.github.io/RAIN-GS/)
 * [eagles](https://github.com/Sharath-girish/efficientgaussian)
 * [pixelgs](https://arxiv.org/abs/2403.15530)
 * [opgs](https://arxiv.org/html/2402.00752v2)
 * [gspro](https://github.com/kcheng1021/GaussianPro/blob/main)
 * [lightgs](https://github.dev/VITA-Group/LightGaussian/blob/main/)
 * [vastgs](https://vastgaussian.github.io/)
 * [octgs](https://github.com/city-super/Octree-GS/tree/main)
 * [sa-gs](https://github.com/zsy1987/SA-GS/tree/master)
 * [BAGS](https://nwang43jhu.github.io/BAGS/)
 * [4dgs](https://github.com/fudan-zvg/4d-gaussian-splatting)
 * [frosting](https://anttwo.github.io/frosting/)
 * [gaustudio](https://github.com/GAP-LAB-CUHK-SZ/gaustudio)
 * [gs2mesh](https://gs2mesh.github.io/)
 * [nep](https://www.whyy.site/paper/nep)

[UserGuide]: docs/userGuide.md    
[DevGuide]:  docs/devGuide.md
[docs]: docs
[diverse]: diverse
[external]: external
[editor]: application/editor
[diverseshot-cli]: application/diverseshot-cli