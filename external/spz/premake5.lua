project "spz"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	files
	{
		"src/**.h",
        "src/**.hpp",
		"src/**.cc",
        "src/**.c",
	}

	includedirs
	{
		"include/",
	}

	externalincludedirs
	{
		"%{IncludeDir.pkg}",
	}

    libdirs
    {
        "%{LibraryDir.pkg}",
    }
	links
	{
		"zlib"
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