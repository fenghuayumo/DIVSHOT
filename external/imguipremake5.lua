project "imgui"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "Off"

	files
	{
		"imgui/imconfig.h",
		"imgui/imgui.h",
		"imgui/imgui.cpp",
		"imgui/imgui_draw.cpp",
		"imgui/imgui_internal.h",
		"imgui/imgui_widgets.cpp",
		"imgui/imgui_tables.cpp",
		"imgui/imstb_rectpack.h",
		"imgui/imstb_textedit.h",
		"imgui/imstb_truetype.h",
		"imgui/imgui_demo.cpp",
		"imgui/Plugins/ImGuizmo.h",
		"imgui/Plugins/ImGuizmo.cpp",
		--"imgui/Plugins/ImCurveEdit.h",
		--"imgui/Plugins/ImCurveEdit.cpp",
		"imgui/Plugins/ImTextEditor.h",
		"imgui/Plugins/ImTextEditor.cpp",
		"imgui/Plugins/imcmd_command_palette.cpp",
		"imgui/Plugins/imcmd_fuzzy_search.cpp",
		"imgui/Plugins/implot/implot.h",
		"imgui/Plugins/implot/implot.cpp",
		"imgui/Plugins/implot/implot_internal.h",
		"imgui/Plugins/implot/implot_items.cpp"
	}

	includedirs
	{
		"imgui/",
		"../",
		"../.."
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