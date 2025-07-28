project "nanobind"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	files
	{
		"src/**.h",
		"src/**.cpp",
	}

	includedirs
	{
		"src/",
		"include/",
        "ext/robin_map/include/"
	}
    externalincludedirs
	{
        "%{IncludeDir.PY3_PATH}/include",
    }
    libdirs
    {
        "%{IncludeDir.PY3_PATH}/libs",
    }

	filter "system:windows"
		systemversion "latest"

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