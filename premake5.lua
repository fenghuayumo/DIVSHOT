require 'Scripts/premake-utilities/premake-defines'
require 'Scripts/premake-utilities/premake-common'
require 'Scripts/premake-utilities/premake-triggers'
require 'Scripts/premake-utilities/premake-settings'
require 'Scripts/premake-utilities/android_studio'

if os.target() == "windows" or os.taget == "linux" then
require 'Scripts/premake-utilities/premake5-cuda/premake5-cuda'
end

include "premake-dependencies.lua"
--require 'Scripts/premake-utilities/premake-vscode/vscode'

function getCudaVersion()
	local nvccOutput = io.popen("nvcc --version"):read("*a")
	local cudaVersion = nvccOutput:match("Cuda compilation tools, release (%d+%.%d+)")
	if cudaVersion then
	print("Detected CUDA Version: " .. cudaVersion)
	else
	print("CUDA version could not be detected.")
	end
	return cudaVersion
end
if os.target() == "windows" or os.taget == "linux" then
cudaBuildCustomizations = "BuildCustomizations/CUDA " .. getCudaVersion()
end
-- print(cudaBuildCustomizations)

root_dir = os.getcwd()

Arch = ""

if _OPTIONS["arch"] then
	Arch = _OPTIONS["arch"]
else
	if _OPTIONS["os"] then
		_OPTIONS["arch"] = "arm"
		Arch = "arm"
	else
		_OPTIONS["arch"] = "x64"
		Arch = "x64"
	end
end

workspace( settings.workspace_name )
	startproject "SplatX"
	flags 'MultiProcessorCompile'
	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
	targetdir ("bin/%{outputdir}/")
	objdir ("bin-int/%{outputdir}/obj/")

	gradleversion "com.android.tools.build:gradle:7.0.0"

	if Arch == "arm" then 
		architecture "ARM"
	elseif Arch == "x64" then 
		architecture "x86_64"
	elseif Arch == "x86" then
		architecture "x86"
	end

	print("Arch = ", Arch)

	configurations
	{
		"Debug",
		"Release",
		"Production"
	}

	assetpacks
	{
		["pack"] = "install-time",
	}
	
	group "external"
		require("external/lua/premake5")
			SetRecommendedSettings()
		require("external/imguipremake5")
			SetRecommendedSettings()
		require("external/freetype/premake5")
			SetRecommendedSettings()
		require("external/SPIRVCrosspremake5")
			SetRecommendedSettings()
		require("external/spdlog/premake5")
			SetRecommendedSettings()
		require("external/ModelLoaders/meshoptimizer/premake5")
			SetRecommendedSettings()
		-- require("external/msdf-atlas-gen/msdfgen/premake5")
		-- 	SetRecommendedSettings()
		require("external/msdf-atlas-gen/premake5")
			SetRecommendedSettings()
		require("external/stbpremake5")
			SetRecommendedSettings()	
		require("external/tinyplypremake5")
			SetRecommendedSettings()	
		require("external/pugixml/premake5")
			SetRecommendedSettings()

		require("external/nanobind/premake5")
			SetRecommendedSettings()
		require("external/ozz-animation/premake5")
			SetRecommendedSettings()
		if not os.istarget(premake.IOS) and not os.istarget(premake.ANDROID) then
			require("external/GLFWpremake5")
			SetRecommendedSettings()
		end
		if os.istarget(premake.MACOSX) then
			require("external/MoltenVK/MoltenVKShaderConverter/premake5")
			SetRecommendedSettings()
		end
		require("external/tinygsplatpremake5")
			SetRecommendedSettings()
		require("external/json/premake5")
			SetRecommendedSettings()
	filter {}
	group ""

	include "diverse/diverse_base/premake5"
	include "diverse/premake5"

	group ""
	include "application/editor/premake5"
	include "application/splatx-cli/premake5"
