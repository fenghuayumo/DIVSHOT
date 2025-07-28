
                
<div align="center">

# SplatX 
** 3d gaussian splat  client training and viewer

</div>

## Project structure
<!-- 
*  diverse
   *  the core rendering lib
*  splatx
   *  the editor exe
   *   -->
|Directory                                  |Details                                                                    |
|-------------------------------------------|---------------------------------------------                              |
|[/diverse][diverse]                        |the core rendering lib including render lib an other useful function       |
|[/application/editor][editor]                |the editor exe, main  program                                  |
|[/external][external]                      |external lib                            |
## Building and running
```
git clone https://github.com/fenghuayumo/SplatX.git
git submodule update --init --recursive
```

#### Windows 
Install Dev Tool 
* 1. VS2022
* 2. Cuda SDK
* 3. Vulkan SDK

* Run scripts/GenerateVS.bat to generate a visual studio project.

#### Mac
Ony support arm arch.
If you're using [Homebrew](https://brew.sh), you can install Cmake/OpenCV/Pytorch by running:

```bash
brew install cmake
brew install opencv
brew install pytorch
```

```bash
brew install \
    cmake \
    ninja \
    boost \
    eigen \
    flann \
    freeimage \
    metis \
    glog \
    googletest \
    ceres-solver \
    qt5 \
    glew \
    cgal \
    sqlite3
```

M1/M2/M3 Macs: using cmake to compile, just execute the following command.
using cmake generate xcode

```
mkdir build
cd build
cmake -G Xcode \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 \
    -DDIVERSE_DEPLOY=1 \
    -DCMAKE_BUILD_TYPE="Production" \
    -DCMAKE_OSX_ARCHITECTURES="arm64;" \
    -DGPU_RUNTIME=MPS \
    ..
```
or using bash scripts to compile
```
scripts/create-dmg.sh
```
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
 * [rain-gs](https://ku-cvlab.github.io/RAIN-GS/)
 * [eagles](https://github.com/Sharath-girish/efficientgaussian)
 * [pixelgs](https://arxiv.org/abs/2403.15530)
 * [opgs](https://arxiv.org/html/2402.00752v2)
 * [lightgs](https://github.dev/VITA-Group/LightGaussian/blob/main/)
 * [octgs](https://github.com/city-super/Octree-GS/tree/main)
 * [sa-gs](https://github.com/zsy1987/SA-GS/tree/master)
 * [4dgs](https://github.com/fudan-zvg/4d-gaussian-splatting)
 * [gaustudio](https://github.com/GAP-LAB-CUHK-SZ/gaustudio)
 * [gs2mesh](https://gs2mesh.github.io/)
 * [supersplat](https://github.com/playcanvas/supersplat)

[diverse]: diverse
[external]: external
[editor]: application/editor
[splatx-cli]: application/splatx-cli