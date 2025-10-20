project "diverse_lauch"
	kind "ConsoleApp"
	language "C++"
	editandcontinue "Off"
		
	files
	{
		"**.h",
		"**.cpp",
		"src/**.h",
		"src/**.cpp"
	}

	externalincludedirs
	{
		"%{IncludeDir.stb}",
		"%{IncludeDir.external}",
		"%{IncludeDir.spdlog}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.gstrain}",
		"%{IncludeDir.gstrain_utils}",
		"%{IncludeDir.CUDA_PATH}",
		"%{IncludeDir.cli11}",
		"%{IncludeDir.cereal}",
		"%{IncludeDir.opencv}",
		"%{IncludeDir.diverse_base}",
		"%{IncludeDir.LibTorch}",
        "%{IncludeDir.LibTorch}/torch/csrc/api/include",
	}
	defines
	{
		"SPDLOG_COMPILED_LIB",
		"GLM_FORCE_INTRINSICS",
		"GLM_FORCE_DEPTH_ZERO_TO_ONE",
		"GLM_FORCE_SWIZZLE",
	}
	openmp "On"
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
		}

		libdirs
		{
		}

		links
		{
            "stbimage",
            "spdlog",
            "diverse_base",
		}

		-- postbuildcommands { "xcopy /Y /C \"..\\External\\OpenAL\\libs\\Win32\\OpenAL32.dll\" \"$(OutDir)\"" } 

		disablewarnings { 4307 }

	filter "system:macosx"
		cppdialect "C++20"
		staticruntime "Off"
		systemversion "13.3"
		editandcontinue "Off"
		
		xcodebuildresources { "assets.xcassets", "libMoltenVK.dylib" }

		xcodebuildsettings
		{
			['ARCHS'] = false,
			['CODE_SIGN_IDENTITY'] = '',
			['INFOPLIST_FILE'] = '../diverse/source/platform/macos/info.plist',
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
			"USE_METAL",
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
			--"../diverse/External/vulkan/libs/macOS/libMoltenVK.dylib"
		}

		links
		{
			"glfw",
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
			"DS_IMGUI",
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
			"external/vulkan/libs/ios/libMoltenVK.a"
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
			['INFOPLIST_FILE'] = '../diverse/source/diverse/platform/iOS/Client/Info.plist',
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
			"../diverse/source/platform/ios/client",
			"assets.xcassets",
            "shaders",
            "meshes",
            "scenes",
            "scripts",
            "sounds",
			"textures",
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
			"DS_VOLK",
			"USE_CUDA"
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
				"../diverse/Assets/Shaders",
				"../diverse/source/diverse/platform/Android/**",
				"../ExampleProject/Assets/Scenes",
				"../ExampleProject/Assets/Scripts",
				"../ExampleProject/Assets/Meshes",
				"../ExampleProject/Assets/Sounds",
				"../ExampleProject/Assets/Textures",
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
