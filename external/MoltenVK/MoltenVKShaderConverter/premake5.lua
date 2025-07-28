project "mvkshaderconverter"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	files
	{
        "**.h",
        "**.cpp",
        "**.mm",
	}

	includedirs
	{
		"../",
		"../.."
	}

    externalincludedirs
	{
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.external}",
		"%{IncludeDir.SpirvCross}"
    }
    links
	{
		"SpirvCross",
    }
    defines
    {
        "MVK_EXCLUDE_SPIRV_TOOLS"
    }
	filter "system:windows"
		systemversion "latest"

	filter "system:linux"
		pic "On"
		systemversion "latest"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

	filter "configurations:Production"
		runtime "Release"
		optimize "on"