project "editor_api"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"

    files {
        "include/**.h",
        "source/**.h",
        "source/**.cpp",
    }

    includedirs {
        "include",
        "source",
    }

    defines {
        "EDITOR_API_EXPORT",
        "EDITOR_API_SHARED",
    }

    filter "system:windows"
        defines { "EDITOR_API_SHARED" }
    filter {}

project "divshot-engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"

    files {
        "source/engine_host_main.cpp",
    }

    includedirs {
        "include",
    }

    links {
        "editor_api",
    }

    dependson { "editor_api" }

    filter "configurations:Debug"
        targetdir ("../../bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/")
    filter "configurations:Release or configurations:Production"
        targetdir ("../../bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/")
    filter {}
