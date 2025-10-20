project "runtime"
	kind "WindowedApp"
	language "C++"

	files
	{
		"**.h",
		"**.cpp",
	}

	externalincludedirs
	{
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
	}

	includedirs
	{
		"../../diverse/source/diverse",
	}

	links
	{
		"diverse",
		"stbimage",
		"lua",
		"box2d",
		"imgui",
		"freetype",
		"SpirvCross",
		"spdlog",
		"meshoptimizer",
		"msdf-atlas-gen"
	}
	libdirs
	{
		"%{LibraryDir.opencv}"
	}
	defines
	{
		"SPDLOG_COMPILED_LIB",
		"GLM_FORCE_INTRINSICS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_SWIZZLE"
	}

	filter { "files:external/**"}
		warnings "Off"

	filter 'architecture:x86_64'
		defines { "USE_VMA_ALLOCATOR"}

	filter "system:windows"
		cppdialect "C++20"
		staticruntime "Off"
		systemversion "latest"
		conformancemode "on"
		
		defines
		{
			"DS_PLATFORM_WINDOWS",
			"DS_RENDER_API_OPENGL",
			"DS_RENDER_API_VULKAN",
			"VK_USE_PLATFORM_WIN32_KHR",
			"WIN32_LEAN_AND_MEAN",
			"_CRT_SECURE_NO_WARNINGS",
			"_DISABLE_EXTENDED_ALIGNED_STORAGE",
			"_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING",
			"DS_VOLK"
		}

		libdirs
		{
			"../external/OpenAL/libs/Win32"
		}

		links
		{
			"glfw",
			"OpenGL32",
			"OpenAL32",
			"opencv_world490.lib"
		}

		postbuildcommands { "xcopy /Y /C \"../external\\OpenAL\\libs\\Win32\\OpenAL32.dll\" \"$(OutDir)\"" } 

		disablewarnings { 4307 }

	filter "system:macosx"
		cppdialect "C++17"
		staticruntime "Off"
		systemversion "11.0"
		editandcontinue "Off"
		
		xcodebuildresources { "Assets.xcassets", "libMoltenVK.dylib" }

		xcodebuildsettings
		{
			['ARCHS'] = false,
			['CODE_SIGN_IDENTITY'] = 'Mac Developer',
			['INFOPLIST_FILE'] = '../diverse/source/platform/macos/info.plist',
			['ASSETCATALOG_COMPILER_APPICON_NAME'] = 'AppIcon',
			['CODE_SIGN_IDENTITY'] = ''
			--['ENABLE_HARDENED_RUNTIME'] = 'YES'
		}

		if settings.enable_signing then
		xcodebuildsettings
		{
			['PRODUCT_BUNDLE_IDENTIFIER'] = settings.bundle_identifier,
			['CODE_SIGN_IDENTITY'] = 'Mac Developer',
			['DEVELOPMENT_TEAM'] = settings.developer_team
		}
		end

		defines
		{
			"DS_PLATFORM_MACOS",
			"DS_PLATFORM_UNIX",
			"DS_RENDER_API_OPENGL",
			"DS_RENDER_API_VULKAN",
			"VK_EXT_metal_surface",
			"DS_IMGUI",
			"DS_VOLK"
		}

		linkoptions
		{
			"-framework OpenGL",
			"-framework Cocoa",
			"-framework IOKit",
			"-framework CoreVideo",
			"-framework OpenAL",
			"-framework QuartzCore"
		}

		files
		{
			"../Resources/AppIcons/Assets.xcassets"
			-- "external/vulkan/libs/macOS/libMoltenVK.dylib"
		}

		links
		{
			"glfw",
			"libopencv_videoio.dylib",
			--   "libopencv_video.4.9.0.dylib",
			  "libopencv_core.dylib",
			  "libopencv_imgproc.dylib",
			  "libopencv_highgui.dylib",
			  "libopencv_calib3d.dylib",
			  "libopencv_imgcodecs.dylib"
		}

		SetRecommendedXcodeSettings()

	filter "system:ios"
		cppdialect "C++17"
		staticruntime "Off"
		systemversion "latest"
		kind "WindowedApp"
		targetextension ".app"
		editandcontinue "Off"
		
		defines
		{
			"DS_PLATFORM_IOS",
			"DS_PLATFORM_MOBILE",
			"DS_PLATFORM_UNIX",
			"DS_RENDER_API_VULKAN",
			"VK_USE_PLATFORM_IOS_MVK",
			"DS_IMGUI"
		}

		links
		{
			"QuartzCore.framework",
			"Metal.framework",
            "MetalKit.framework",
        	"IOKit.framework",
        	"CoreFoundation.framework",
			"CoreVideo.framework",
			"CoreGraphics.framework",
			"UIKit.framework",
			"OpenAL.framework",
			"AudioToolbox.framework",
			"Foundation.framework",
			"SystemConfiguration.framework",
		}

		linkoptions
		{
			"external/vulkan/libs/iOS/libMoltenVK.a"
		}

		files
		{
			"../Resources/AppIcons/Assets.xcassets",
		}

		xcodebuildsettings
		{
			['ARCHS'] = '$(ARCHS_STANDARD)',
			['ONLY_ACTIVE_ARCH'] = 'NO',
			['SDKROOT'] = 'iphoneos',
			['TARGETED_DEVICE_FAMILY'] = '1,2',
			['SUPPORTED_PLATFORMS'] = 'iphonesimulator iphoneos',
			['CODE_SIGN_IDENTITY[sdk=iphoneos*]'] = '',
			['IPHONEOS_DEPLOYMENT_TARGET'] = '13.0',
			['INFOPLIST_FILE'] = '../diverse/source/diverse/platform/iOS/Client/Info.plist',
	['ASSETCATALOG_COMPILER_APPICON_NAME'] = 'AppIcon'
}

if settings.enable_signing then
xcodebuildsettings
{
	['PRODUCT_BUNDLE_IDENTIFIER'] = settings.bundle_identifier,
	['CODE_SIGN_IDENTITY[sdk=iphoneos*]'] = 'iPhone Developer',
	['DEVELOPMENT_TEAM'] = settings.developer_team
}
end

		if _OPTIONS["teamid"] then
			xcodebuildsettings {
				["DEVELOPMENT_TEAM"] = _OPTIONS["teamid"]
			}
		end

		if _OPTIONS["tvOS"] then
			xcodebuildsettings {
				["SDKROOT"] = _OPTIONS["tvos"],
				['TARGETED_DEVICE_FAMILY'] = '3',
				['SUPPORTED_PLATFORMS'] = 'tvos'
			}
		end

		linkoptions { "-rpath @executable_path/Frameworks" }

		excludes
		{
			("**.DS_Store")
		}

		xcodebuildresources
		{
			"../diverse/source/platform/iOS/Client",
			"Assets.xcassets",
            "Shaders",
            "Meshes",
            "Scenes",
            "Scripts",
            "Sounds",
            "Textures",
            "Example.lmproj"
		}
		SetRecommendedXcodeSettings()

	filter "system:linux"
		cppdialect "C++17"
		staticruntime "Off"
		systemversion "latest"

		defines
		{
			"DS_PLATFORM_LINUX",
			"DS_PLATFORM_UNIX",
			"DS_RENDER_API_OPENGL",
			"DS_RENDER_API_VULKAN",
			"VK_USE_PLATFORM_XCB_KHR",
			"DS_IMGUI",
			"DS_VOLK"
		}

		buildoptions
		{
			"-fpermissive",
			"-Wattributes",
			"-fPIC",
			"-Wignored-attributes",
			"-Wno-psabi"
		}

		links { "X11", "pthread", "dl", "atomic", "openal", "glfw"}

		linkoptions { "-L%{cfg.targetdir}", "-Wl,-rpath=\\$$ORIGIN"}

		filter {'system:linux', 'architecture:x86_64'}
			buildoptions
			{
				"-msse4.1",
			}

	filter "configurations:Debug"
defines { "DS_DEBUG", "_DEBUG","TRACY_ENABLE","DS_PROFILE_ENABLED","TRACY_ON_DEMAND" }
		symbols "On"
		runtime "Debug"
		optimize "Off"

	filter "configurations:Release"
defines { "DS_RELEASE", "NDEBUG", "TRACY_ENABLE", "DS_PROFILE_ENABLED","TRACY_ON_DEMAND"}
		optimize "Speed"
		symbols "On"
		runtime "Release"

	filter "configurations:Production"
		defines { "DS_PRODUCTION", "NDEBUG" }
		symbols "Off"
		optimize "Full"
		runtime "Release"
