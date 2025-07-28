VULKAN_SDK = os.getenv("VULKAN_SDK")
LIBTORCH_SDK = os.getenv("LIBTORCH_SDK")
CUDA_PATH = os.getenv("CUDA_PATH")
PY3_PATH = os.getenv("PY3_PATH")
OPEN3D_PATH = os.getenv("OPEN3D_PATH")
PACKAGE_PATH = os.getenv("PACKAGE_PATH")

IncludeDir = {}
IncludeDir["entt"] = "%{wks.location}/external/entt/src/"
IncludeDir["GLFW"] = "%{wks.location}/external/glfw/include/"
IncludeDir["Glad"] = "%{wks.location}/external/glad/include/"
IncludeDir["lua"] = "%{wks.location}/external/lua/src/"
IncludeDir["stb"] = "%{wks.location}/external/stb/"
IncludeDir["vulkan"] = "%{wks.location}/external/vulkan/"
IncludeDir["diverse_base"] = "%{wks.location}/diverse/diverse_base/source"
IncludeDir["diverse"] = "%{wks.location}/diverse/source"
IncludeDir["external"] = "%{wks.location}/external/"
IncludeDir["ImGui"] = "%{wks.location}/external/imgui/"
IncludeDir["freetype"] = "%{wks.location}/external/freetype/include"
IncludeDir["SpirvCross"] = "%{wks.location}/external/vulkan/SPIRV-Cross"
IncludeDir["cereal"] = "%{wks.location}/external/cereal/include"
IncludeDir["spdlog"] = "%{wks.location}/external/spdlog/include"
IncludeDir["tinyply"] = "%{wks.location}/external/tinyply"
IncludeDir["glm"] = "%{wks.location}/external/glm"
IncludeDir["tinygsplat"] = "%{wks.location}/external/tinygsplat"
IncludeDir["msdf_atlas_gen"] = "%{wks.location}/external/msdf-atlas-gen/msdf-atlas-gen"
IncludeDir["msdfgen"] = "%{wks.location}/external/msdf-atlas-gen/msdfgen"
IncludeDir["cli11"] = "%{wks.location}/external/CLI11/include/"
IncludeDir["colmap"] = "%{wks.location}/external/colmap/include/"
IncludeDir["pybind11"] = "%{wks.location}/external/pybind11/include/"
IncludeDir["nanobind"] = "%{wks.location}/external/nanobind/include/"
IncludeDir["rapidcsv"] = "%{wks.location}/external/rapidcsv"
IncludeDir["pugixml"] = "%{wks.location}/external/pugixml/src/"
IncludeDir["spz"] = "%{wks.location}/external/spz/include/"
IncludeDir["ozz"] = "%{wks.location}/external/ozz-animation/include/"
IncludeDir["tomasakeninemoeller"] = "%{wks.location}/external/tomasakeninemoeller/include/"
IncludeDir["CUDA_PATH"] = "%{CUDA_PATH}/include/"
if os.target() == "windows" then
IncludeDir["opencv"] = "%{wks.location}/external/opencv4_9/include/"
IncludeDir["LibTorch"] = "%{PY3_PATH}/Lib/site-packages/torch/include"
IncludeDir["Package"] = "%{PACKAGE_PATH}/include/"
elseif os.target() == "macosx" then
IncludeDir["opencv"] = "/opt/homebrew/opt/opencv/include/opencv4/"
IncludeDir["LibTorch"] = "/opt/homebrew/opt/pytorch/include"
end
IncludeDir["metalir"] = "%{wks.location}/external/metal_irconverter_runtime/";
-- IncludeDir["ozz"] = "%{wks.location}/external/ozz-animation/include"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["PY3_PATH"] = "%{PY3_PATH}/"
IncludeDir["OPEN3D_PATH"] = "%{OPEN3D_PATH}/include"
IncludeDir["pkg"] = "%{wks.location}/external/pkg/include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["CUDA_PATH"] = "%{CUDA_PATH}/lib/x64"
LibraryDir["pkg"] = "%{wks.location}/external/pkg/liblink"
if os.target() == "windows" then
LibraryDir["LibTorch"] = "%{PY3_PATH}/Lib/site-packages/torch/lib"
LibraryDir["opencv"] = "%{wks.location}/external/opencv4_9/linklib"
LibraryDir["spz"] = "%{wks.location}/external/spz/liblink/win/"
LibraryDir["Package"] = "%{PACKAGE_PATH}/lib/"
elseif os.target() == "macosx" then
LibraryDir["opencv"] = "/opt/homebrew/opt/opencv/lib" 
LibraryDir["LibTorch"] = "/opt/homebrew/opt/pytorch/lib"
end
-- print("%{CUDA_PATH}")
-- /opt/homebrew/Cellar/opencv/4.9.0_7/lib

-- /opt/homebrew/Cellar/opencv/4.9.0_7/include/opencv4