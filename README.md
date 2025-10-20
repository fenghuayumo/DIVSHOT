<div align="center">

# diverse 
**a lightweight radiance field renderer, which will combine traditional rennder and neural renderer to render beatiful images**

</div>

## Screenshots
![diverse](/screenshots/screenshot.png?raw=true)
![diverse](/screenshots/screenshot2.png?raw=true)
![diverse](/screenshots/screenshot3.png?raw=true)
## 🎥 Videos
![diverse](/screenshots/video1.mp4?)

## User Guide
- [UserGuide][UserGuide] user documentation
- [DevGuide][DevGuide] develop extend docs
## Video Examples

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
<!-- *  gsplatrast
   *  the core gsplat rasterizer forward and backward training lib
*  diverse
   *  the core rendering lib
*  diverseshot
   *  the editor exe
*  gstrain
   *  the core gsplat training lib which use gsplatrast lib to forward and backward render, contains data loader process
*  gstrain_utils
   *  the gstrain utils lib which will be used in gstrain project.
*  pygstrain_utils
   *  the python binding for gstrain_utils
*  hybrid_gstrain
   *  Similar to gstrain, but trained using a mixture of Python and C++ code so that the editor will not depend on torch dll directly.
   *   -->
|Directory                                  |Details                                                                    |
|-------------------------------------------|---------------------------------------------                              |
|[/docs][docs]                              |_Documentation for showcased tech_                                         |
|[/diverse_utils/gsplatrast][gsplatrast]                  |the core gsplat rasterizer forward and backward training lib               |
|[/diverse_utils/gstrain][gstrain]                        |the core gsplat training lib which use gsplatrast lib to forward and backward render, contains data loader process                                   |
|[/diverse_utils/gstrain_utils][gstrain_utils]            |the gstrain utils lib which will be used in gstrain project.               |
|[/diverse_utils/pygstrain_utils][pygstrain_utils]        |the python binding for gstrain_utils            |
|[/diverse_utils/hybrid_gstrain][hybrid_gstrain]          |Similar to gstrain, but trained using a mixture of Python and C++ code so that the editor will not depend on torch dll directly.              |
|[/diverse][diverse]                        |the core rendering lib including render lib an other useful function       |
|[/application/diverseshot][diverseshot]                |the editor exe, main  program                                               |
|[/python][python]                          |python code interface             |
|[/application/diverseshot-cli][diverseshot-cli]                |gsplat training console exe             |
|[/external][external]                      |external lib                            |
## Building and running
```
git clone https://github.com/fenghuayumo/diverse.git
git submodule update --init --recursive
```

To build `diverse` you should open the scripts folder, Then, doing the following operations

0. install the SDK package mentioned earlier.
1. make sure you have set CUDA_PATH and PY3_PATH enviroment variable.
2. install pytorch (the corresponding cuda version)
3. select corrsponding platform folder, double click the .bat file. this will make the projects
4. you should select the diverseshot as startup project, then build this project.
5. copy the corresponding dll file to bin folder. Here is the install package which contains sfm dll file(https://github.com/fenghuayumo/diverse/releases), you should copy sfm and torch related dll file to bin folder(you can download from pytorch official website).
### Note:
    if you only want to build a gsplat/mesh render viewer, you can disable gstrain compile by remove DS_SPLAT_TRAIN macro and remove gstrain lib dependencies in premake5.lua config in the editor folder.

#### Windows 
* Run Scripts/GenerateVS.bat to generate a visual studio project.
* Build with Python (optional if you want to use python interface)
  * fisrt build gsplatrast, then build gstrain_utils with Production version, you can binding these lib to python using "python setup.py install" cmd.
  * run with "python main.py configs/nerf.yaml --gpu 0 --train sourcePath="xxxx" modelPath="xxxx" 

##### Python Run Mode
- [tiny-cuda-nn](https://github.com/NVlabs/tiny-cuda-nn)
- [nvdiffrast](https://nvlabs.github.io/nvdiffrast/)
- [largestep](https://github.com/rgl-epfl/large-steps-pytorch.git)
- generate depth images (optional)
```
cd diverse/python
cd depth_anything
python run.py --encoder vitl --pred-only --grayscale --img-path <path to input images> --outdir <output path> (option)  
```

```
cd diverse
cd diverse_utils/gsplatrast
python setup.py install

cd diverse_utils/pygstrain_utils
python setup.py install

pip3 install trimesh \
            open3d \
            nerfacc \
            omegaconf \
            xatlas \
            loguru \
            omegaconf \
            PyMCubes \

python main.py --config configs/gsplat.yaml --gpu 0 --train sourcePath="xxxx" modelPath="xxxx" 
```

#### Linux
```
sudo apt-get install -y g++-11 libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libopenal-dev mesa-common-dev
cd diverse
tools/linux/premake5 gmake2
make -j8 # config=release
```

#### Mac
Ony support arm arch.
If you're using [Homebrew](https://brew.sh), you can install Cmake/OpenCV/Pytorch by running:

```bash
brew install cmake
brew install opencv
brew install pytorch
```
Note: first compile colmap

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

git clone https://github.com/fenghuayumo/colmap.git
cd colmap
mkdir build
cd build
cmake .. -GNinja -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"
ninja
sudo ninja install

sudo cp src/colmap/dvscolmap/libcolmap_dvs.dylib diverse/external/colmap/liblink/mac/libcolmap_dvs.dylib
```

You will also need to install Xcode and the Xcode command line tools to compile with metal support (otherwise, gstrain will build with CPU acceleration only):
1. Install Xcode from the Apple App Store.
2. Install the command line tools with `xcode-select --install`. This might do nothing on your machine.
3. If `xcode-select --print-path` prints `/Library/Developer/CommandLineTools`,then run `sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer`.


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
#### iOS


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
[diverseshot]: application/diverseshot
[gstrain]: diverse_utils/gstrain
[gstrain_utils]: diverse_utils/gstrain_utils
[pygstrain_utils]: diverse_utils/pygstrain_utils
[gsplatrast]: diverse_utils/gsplatrast
[python]: python
[hybrid_gstrain]: diverse_utils/hybrid_gstrain
[diverseshot-cli]: application/diverseshot-cli