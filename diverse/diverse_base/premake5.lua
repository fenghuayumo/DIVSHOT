newoption 
{
	trigger = "time-trace",
	description = "Build with -ftime-trace (clang only)"
}

project "diverse_base"
	kind "StaticLib"
	language "C++"
	editandcontinue "Off"
	staticruntime "Off"

	files
	{
		"source/**.h",
		"source/**.c",
		"source/**.cpp",
		-- "assets/shaders/**.hlsl"
	}

	includedirs
	{
		"",
		"../",
		"source/",
	}

	externalincludedirs
	{
		"%{IncludeDir.entt}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.OpenAL}",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.external}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.freetype}",
		"%{IncludeDir.SpirvCross}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdf_atlas_gen}",
		"%{IncludeDir.diverse}",
		"%{IncludeDir.opencv}",
		"%{IncludeDir.tinygsplat}", 
		"%{IncludeDir.metalir}",
		"%{IncludeDir.nanobind}",
		"%{IncludeDir.pybind11}",
		"%{IncludeDir.ozz}",
		"%{IncludeDir.PY3_PATH}/include",
		"%{IncludeDir.wgpu}",
	}
	libdirs
    {
        "%{IncludeDir.PY3_PATH}/libs",
    }
	
	links
	{
		"spdlog",
	}

	defines
	{
		"DS_ENGINE",
		"FREEIMAGE_LIB",
		"SPDLOG_COMPILED_LIB",
		"GLM_FORCE_INTRINSICS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_SWIZZLE",
		"USE_OPENMP",
	}
	openmp "On"
	filter "options:time-trace"
		buildoptions {"-ftime-trace"}
		linkoptions {"-ftime-trace"}

	filter 'architecture:x86_64'
		defines { "USE_VMA_ALLOCATOR"}

	filter "system:windows"
		cppdialect "C++20"
		systemversion "latest"
		disablewarnings { 4307 }
		characterset ("Unicode")
		conformancemode "on"

		defines
		{
			"DS_PLATFORM_WINDOWS",
			"WIN32_LEAN_AND_MEAN",
			"_CRT_SECURE_NO_WARNINGS",
			"_DISABLE_EXTENDED_ALIGNED_STORAGE",
			"_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING",
			"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING",
		}

		links
		{
			"Dbghelp",
			"Dwmapi.lib",
		}

		buildoptions
		{
			"/bigobj"
		}

		filter 'files:external/**.cpp'
			flags  { 'NoPCH' }
		filter 'files:external/**.c'
			flags  { 'NoPCH' }

	filter "system:macosx"
		cppdialect "C++20"
		systemversion "13.3"

		defines
		{
			"DS_PLATFORM_MACOS",
		}

		links
		{
			"QuartzCore.framework",
			"Metal.framework",
			"Cocoa.framework",
        	"IOKit.framework",
        	"CoreFoundation.framework",
			"CoreVideo.framework",
			"OpenAL.framework",
			"SystemConfiguration.framework",
			-- "libmetalirconverter.dylib",
	
		}

		libdirs
		{
			"../bin/**",
			-- "%{wks.location}/external/MoltenVK/"
		}

		buildoptions
		{
			"-Wno-attributes",
			"-Wno-nullability-completeness",
			"-fdiagnostics-absolute-paths"
		}

		SetRecommendedXcodeSettings()

		filter 'files:external/**.cpp'
			flags  { 'NoPCH' }
		filter 'files:external/**.c'
			flags  { 'NoPCH' }
		filter 'files:source/**.m'
			flags  { 'NoPCH' }
		filter 'files:source/**.mm'
			flags  { 'NoPCH' }

	filter "system:ios"
		cppdialect "C++20"
		systemversion "latest"
		kind "StaticLib"

		defines
		{
			"DS_PLATFORM_IOS",
			"DS_PLATFORM_MOBILE",
			"DS_PLATFORM_UNIX",
		}

		links
		{
			"QuartzCore.framework",
			"Metal.framework",
        	"IOKit.framework",
        	"CoreFoundation.framework",
			"CoreVideo.framework",
			"OpenAL.framework"
		}

		libdirs
		{
			"../bin/**"
		}

		buildoptions
		{
			"-Wno-attributes",
			"-Wno-nullability-completeness"
		}

		SetRecommendedXcodeSettings()

	filter "system:linux"
		cppdialect "C++20"
		systemversion "latest"

		defines
		{
			"DS_PLATFORM_LINUX",
			"DS_PLATFORM_UNIX",
			"DS_RENDER_API_OPENGL",
			"DS_RENDER_API_VULKAN",
			"VK_USE_PLATFORM_XCB_KHR",
			"DS_IMGUI",
			"DS_VOLK",
			"DS_OPENAL"
		}

		linkoptions{ "-Wl,-rpath=\\$$ORIGIN" }

		libdirs
		{
			"../bin/**",
			"external/openal/libs/linux"
		}

		buildoptions
		{
			"-fpermissive",
			"-fPIC",
			"-Wignored-attributes",
			"-Wno-psabi",
		}

		links { "X11", "pthread"}


		filter 'files:external/**.cpp'
			flags  { 'NoPCH' }
		filter 'files:external/**.c'
			flags  { 'NoPCH' }
		filter 'files:source/**.c'
			flags  { 'NoPCH' }

		filter {'system:linux', 'architecture:x86_64'}
			buildoptions
			{
				"-msse4.1",
			}

	filter "configurations:Debug"
defines { "DS_DEBUG", "_DEBUG","TRACY_ENABLE","DS_PROFILE_ENABLED","TRACY_ON_DEMAND"  }
		symbols "On"
		runtime "Debug"
		optimize "Off"

	filter "configurations:Release"
defines { "DS_RELEASE", "NDEBUG", "TRACY_ENABLE","DS_PROFILE_ENABLED", "TRACY_ON_DEMAND"}
		optimize "Speed"
		symbols "On"
		runtime "Release"

	filter "configurations:Production"
		defines { "DS_PRODUCTION", "NDEBUG" }
		symbols "Off"
		optimize "Full"
		runtime "Release"
