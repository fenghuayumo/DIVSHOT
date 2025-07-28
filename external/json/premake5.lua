project "jsoncpp"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "Off"

	files
	{
		"**.h",
        "**.hpp",
        "src/**.cpp",
	}

	includedirs
	{
		"src/",
	}

	externalincludedirs
	{
		"%{IncludeDir.Package}",
        "%{IncludeDir.external}",
	}

    libdirs
    {
        "%{LibraryDir.Package}",
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