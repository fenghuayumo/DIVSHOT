project "tinygsplat"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	files
	{
		"tinygsplat/tiny_gsplat.hpp",
		"tinygsplat/tiny_gsplat.cpp",
	}

	includedirs
	{
		"tinygsplat/",
		"../",
		"../.."
	}
  externalincludedirs
  {
    "%{IncludeDir.glm}",
    "%{IncludeDir.spz}",
    "%{IncludeDir.tinyply}",
  }
	buildcustomizations {cudaBuildCustomizations}

    if os.target() == "windows" then
      -- Just in case we want the VS CUDA extension to use a custom version of CUDA
      cudaPath "$(CUDA_PATH)"
      cudaFiles { "../external/tinygsplat/**.cu" }
    elseif os.target() == "linux" then
      toolset "nvcc"
      cudaPath "/usr/local/cuda"
      files { "../external/tinygsplat/**.cu" }
      rules {"cu"}
    end

    if os.target() == "windows" or os.target() == "linux" then

        cudaRelocatableCode "On"
        -- Let's compile for all supported architectures (and also in parallel with -t0)
        cudaCompilerOptions {"--generate-code=arch=compute_70,code=[compute_70,sm_70] --generate-code=arch=compute_80,code=[compute_80,sm_80] --extended-lambda --expt-relaxed-constexpr", "-t0", "-std=c++17","-diag-suppress 1388"} 
        cudaFastMath "On"
        cudaIntDir "bin/cudaobj/%{cfg.buildcfg}"
        -- linkoptions { "/INCLUDE:?warp_size@cuda@at@@YAHXZ" }
        --  -Xcompiler="/EHsc -Ob2 /bigobj"
    end
	defines { "GS_DYNAMIC", "GS_ENGINE",}
  libdirs
  {
      -- "%{LibraryDir.spz}",
      "%{LibraryDir.pkg}/"
  }

	filter "system:windows"
		systemversion "latest"
    links
    {
        "spz",
        -- "tinyply",
        "zlib"
    }
	filter "system:linux"
		pic "On"
		systemversion "latest"

    filter "configurations:Debug"
        defines { "_DEBUG" }
        symbols "On"
        runtime "Debug"
        optimize "Off"
    
    filter "configurations:Release"
        defines {"NDEBUG" }
        optimize "Speed"
        symbols "On"
        runtime "Release"
    
    filter "configurations:Production"
        defines { "NDEBUG" }
        symbols "Off"
        optimize "Full"
        runtime "Release"