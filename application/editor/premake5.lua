project "divshot"
	kind "WindowedApp"
	language "C++"
	editandcontinue "Off"
		
	files
	{
		"**.h",
		"**.cpp",
		"source/**.h",
		"source/**.cpp"
	}
	
	includedirs
	{
		"../../diverse/source",
	}

	externalincludedirs
	{
		"%{IncludeDir.entt}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.tinyply}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.OpenAL}",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.vulkan}",
		"%{IncludeDir.external}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.freetype}",
		"%{IncludeDir.SpirvCross}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdf_atlas_gen}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.diverse}",
		"%{IncludeDir.diverse_base}",
		"%{IncludeDir.gstrain}",
		"%{IncludeDir.gstrain_utils}",
		"%{IncludeDir.CUDA_PATH}",
		"%{IncludeDir.opencv}",
		"%{IncludeDir.nanobind}",
		"%{IncludeDir.PY3_PATH}/include",
	}
	libdirs
    {
        "%{IncludeDir.PY3_PATH}/libs",
    }

	links
	{
		"diverse_base",
		"diverse",
		"lua",
		"box2d",
		"imgui",
		"freetype",
		"stbimage",
		"tinyply",
		"SpirvCross",
		"spdlog",
		"meshoptimizer",
		"msdf-atlas-gen",
		"tinygsplat",
	}
	libdirs
	{
		"%{LibraryDir.opencv}"
	}
	defines
	{
		"IMGUI_USER_CONFIG=\"../../diverse/source/imgui/ImUserConfig.h\"",
		"SPDLOG_COMPILED_LIB",
		"GLM_FORCE_INTRINSICS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_SWIZZLE",
		"GS_DYNAMIC", 
		"DS_EDITOR",
		"USE_OPENMP",
		"SPLAT_EDIT"
	}
	openmp "On"
	filter { "files:external/**"}
		warnings "Off"

	filter 'architecture:x86_64'
		defines { "USE_VMA_ALLOCATOR"}

	filter "system:windows"
		cppdialect "C++20"
		staticruntime "Off"
		systemversion "13.3"
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
			"DS_VOLK",
			"DS_SPLAT_TRAIN"
		}

		libdirs
		{
			"../../external/OpenAL/libs/Win32",
			"%{LibraryDir.CUDA_PATH}",
		}

		links
		{
			"glfw",
			"OpenGL32",
			"OpenAL32",
			"gstrain",
			"opencv_world490.lib"
		}

		postbuildcommands { "xcopy /Y /C \"..\\..\\external\\OpenAL\\libs\\Win32\\OpenAL32.dll\" \"$(OutDir)\"" } 

		disablewarnings { 4307 }

	filter "system:macosx"
		cppdialect "C++20"
		staticruntime "Off"
		systemversion "11.0"
		editandcontinue "Off"
		
		xcodebuildresources 
		{ 
			"assets.xcassets", 
			-- "libMoltenVK.dylib",
			-- "libmetalirconverter.dylib",
			-- "gstrain",
			-- "%{targetdir}/libgstrain.dylib",
			-- "%{LibraryDir.opencv}/libopencv_videoio.dylib",
			-- "%{LibraryDir.opencv}/libopencv_core.dylib",
			-- "%{LibraryDir.opencv}/libopencv_imgproc.dylib",
			-- "%{LibraryDir.opencv}/libopencv_highgui.dylib",
			-- "%{LibraryDir.opencv}/libopencv_calib3d.dylib",
			-- "%{LibraryDir.opencv}/libopencv_imgcodecs.dylib",
			-- "%{targetdir}/libtorch_cpu.dylib",
			-- "%{targetdir}/libtorch.dylib",
			-- "%{targetdir}/libc10.dylib",
			-- "%{targetdir}/libcolmap_dll.dylib"
		}

		xcodebuildsettings
		{
			['ARCHS'] = false,
			['CODE_SIGN_IDENTITY'] = '',
			['INFOPLIST_FILE'] = '../../diverse/source/platform/macos/info.plist',
			['ASSETCATALOG_COMPILER_APPICON_NAME'] = 'AppIcon'
			--['ENABLE_HARDENED_RUNTIME'] = 'YES'
		}

		if settings.enable_signing then
		xcodebuildsettings
		{
			['CODE_SIGN_IDENTITY'] = 'Mac Developer',
			['DEVELOPMENT_TEAM'] = settings.developer_team,
				['PRODUCT_BUNDLE_IDENTIFIER'] = settings.bundle_identifier
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
			"DS_VOLK",
			-- "DS_SPLAT_TRAIN"
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
			"../Resources/AppIcons/assets.xcassets",
			--"../diverse/external/vulkan/libs/macOS/libMoltenVK.dylib"
		}

		-- libdirs
		-- {
		-- 	-- "%{wks.location}/external/MoltenVK/",
		-- }

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
			"glfw",
			-- "libmetalirconverter.dylib",
			-- "gstrain",
			"libopencv_videoio.dylib",
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
			"external/vulkan/libs/iOS/libMoltenVK.a",

		}

		--Adding assets from Game Project folder. Needs rework
		files
		{
			"../diverse/source/platform/ios/client/**",
		}

		xcodebuildsettings
		{
			['ARCHS'] = '$(ARCHS_STANDARD)',
			['ONLY_ACTIVE_ARCH'] = 'NO',
			['SDKROOT'] = 'iphoneos',
			['TARGETED_DEVICE_FAMILY'] = '1,2',
			['SUPPORTED_PLATFORMS'] = 'iphonesimulator iphoneos',
			['CODE_SIGN_IDENTITY'] = '',
			['IPHONEOS_DEPLOYMENT_TARGET'] = '13.0',
			['INFOPLIST_FILE'] = '../diverse/source/platform/ios/client/info.plist',
			['ASSETCATALOG_COMPILER_APPICON_NAME'] = 'AppIcon',
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
			"assets.xcassets",
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

		links
		{
			"glfw",
		}

		links { "X11", "pthread", "dl", "atomic", "openal"}

		linkoptions { "-L%{cfg.targetdir}", "-Wl,-rpath=\\$$ORIGIN"}

		filter {'system:linux', 'architecture:x86_64'}
			buildoptions
			{
				"-msse4.1",
			}
	
			
		filter "system:android"
			androidsdkversion "29"
			androidminsdkversion "29"
			cppdialect "C++17"
			staticruntime "Off"
			systemversion "latest"
			kind "WindowedApp"
			targetextension ".apk"
			editandcontinue "Off"
			
			defines
			{
				"DS_PLATFORM_ANDROID",
				"DS_PLATFORM_MOBILE",
				"DS_PLATFORM_UNIX",
				"DS_RENDER_API_VULKAN",
				"DS_IMGUI"
			}
	
			--Adding assets from Game Project folder. Needs rework
			files
			{
				"../diverse/assets/shaders",
				"../diverse/source/platform/Android/**",
				"../ExampleProject/assets/Scenes",
				"../ExampleProject/assets/Scripts",
				"../ExampleProject/assets/Meshes",
				"../ExampleProject/assets/Sounds",
				"../ExampleProject/assets/Textures",
				"../ExampleProject/Example.lmproj"
			}
			
			links
			{
				"log" -- required for c++ logging	
			}
			
			buildoptions
			{
				"-std=c++17" -- flag mainly here to test cmake compile options
			}
			
			linkoptions
			{
				"--no-undefined" -- this flag is used just to cmake link libraries
			}
			
			includedirs
			{
				"h"
			}
			
			androidabis
			{
				"arm64-v8a"
			}
		
			androiddependencies
			{
				"com.android.support:support-v4:27.1.0",
			}
			
			assetpackdependencies
			{
				"pack"
			}

	filter "configurations:Debug"
		defines { "DS_DEBUG", "_DEBUG","TRACY_ENABLE","DS_PROFILE_ENABLED","TRACY_ON_DEMAND" }
		symbols "On"
		runtime "Debug"
		optimize "Off"

	filter "configurations:Release"
defines { "DS_RELEASE", "NDEBUG", "TRACY_ENABLE","DS_PROFILE_ENABLED","TRACY_ON_DEMAND"}
		optimize "Speed"
		symbols "On"
		runtime "Release"

	filter "configurations:Production"
		defines { "DS_PRODUCTION", "NDEBUG" }
		symbols "Off"
		optimize "Full"
		runtime "Release"
