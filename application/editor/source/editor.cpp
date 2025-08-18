
#include "editor.h"
#include "hierarchy_panel.h"
#include "console_panel.h"
#include "inspector_panel.h"
#include "scene_view_panel.h"
#include "scene_preview_panel.h"
#include "keyframe_panel.h"
#include "histogram_panel.h"
#include "post_process_panel.h"
#include "imgui_console_sink.h"
#include "progress_bar.h"
#include "helper_panel.h"
#include "resource_panel.h"
#include "img2d_dataset_panel.h"
#include "texture_paint_panel.h"
#include "embed_resources.h"
#include "pivot.h"
#include "redo_undo_system.h"
#include <scene/camera/editor_camera.h>
#include <utility/timer.h>
#include <engine/application.h>
#include <engine/input.h>
#include <engine/os.h>
#include <core/version.h>
#include <engine/engine.h>
#include <engine/window.h>
#include <scene/scene.h>
#include <scene/scene_manager.h>
#include <scene/entity.h>
#include <scene/entity_manager.h>
#include <scene/sun_controller.h>

#include <events/application_event.h>
#include <core/job_system.h>
#include <backend/drs_rhi/gpu_device.h>

#include <imgui/Plugins/imcmd_command_palette.h>
#include <renderer/debug_renderer.h>
#include <core/ds_log.h>
#include <core/command_line.h>
#include <engine/core_system.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <imgui/imgui_internal.h>
#include <imgui/Plugins/ImGuizmo.h>
#include <imgui/imgui_manager.h>
#include <imgui/imgui_renderer.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <cereal/version.hpp>
#include <scene/component/mesh_model_component.h>
#include <scene/component/gaussian_component.h>
#include <scene/component/gaussian_crop.h>
#include <scene/component/point_cloud_component.h>
#include <scene/component/environment.h>
#include <scene/component/light/directional_light.h>
#include <scene/component/light/point_light.h>
#include <scene/component/light/rect_light.h>
#include <scene/component/light/spot_light.h>
#include <renderer/render_settings.h>
// #define IMOGUIZMO_RIGHT_HANDED
#include <imoguizmo/imoguizmo.hpp>
#include "html_view_template.hpp"
#include "gaussian_edit.h"
#ifdef DS_SPLAT_TRAIN
#include <gaussian_trainer_scene.hpp>
GaussianTrainConfig trainConfig;
#endif

namespace diverse
{
    Editor* Editor::s_Editor = nullptr;
    static std::string projectLocation = "../projects/";
    static std::string layoutFolder = "../layouts/";
    static bool reopenNewProjectPopup = false;
    static bool locationPopupOpened = false;
    static bool locationPopupCanceled = false;
    static bool saveSceneShowAgain = true;
    static bool saveScenePopupCaneceld = false;
    static bool newScenePopup = false;
    static bool saveScenePopup = false;
    static int  exportType = 0;
    static bool gs2mesh_load = false;
    std::string get_resource_path()
    {
        return FileSystem::get_working_directory() + "/../resource/";
    }

    Editor::Editor()
        : Application()
        , ini_file("")
    {
#if DS_ENABLE_LOG
        spdlog::sink_ptr sink = std::make_shared<ImGuiConsoleSink_mt>();

        diverse::debug::Log::add_sink(sink);
#endif

        // Remove?
        s_Editor = this;
    }

    Editor::~Editor()
    {
    }

    void Editor::quit()
    {
        save_editor_settings();

        for (auto panel : panels)
            panel->destroy_graphics_resources();

        
        panels.clear();

        Application::quit();
    }


    bool show_demo_window = true;
    bool show_command_palette = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImCmd::Command toggle_demo_cmd;

    void Editor::init()
    {
        DS_PROFILE_FUNCTION();
#ifdef DS_SPLAT_TRAIN
        gstrain_init();
#endif
        auto& guizmoStyle = ImGuizmo::GetStyle();
        guizmoStyle.HatchedAxisLineThickness = -1.0f;

#ifdef DS_PLATFORM_IOS
        temp_scene_save_file_path = OS::instance()->GetAssetPath();
#else
#ifdef DS_PLATFORM_LINUX
        temp_scene_save_file_path = std::filesystem::current_path().string();
#else
        temp_scene_save_file_path = std::filesystem::temp_directory_path().string();
#endif

#ifdef DS_PLATFORM_MACOS
        projectLocation = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../projects/";
        layoutFolder = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../layouts/";
#else
        projectLocation = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../projects/";
        layoutFolder = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../layouts/";
#endif

        bool deleteIniFile = false;
        CommandLine* cmdline = coresystem::get_command_line();
        if (cmdline->option_bool(Str8Lit("clean_editor_ini")))
        {
           DS_LOG_INFO("deleting editor ini file");
           deleteIniFile = true;
        }

        temp_scene_save_file_path += "diverse/";
        if (!FileSystem::folder_exists(temp_scene_save_file_path))
            std::filesystem::create_directory(temp_scene_save_file_path);

        std::vector<std::string> iniLocation = {
            stringutility::get_file_location(OS::instance()->getExecutablePath()) + "editor.ini",
            stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../editor.ini"
        };
        bool fileFound = false;
        std::string filePath;
        for (auto& path : iniLocation)
        {
            if (FileSystem::file_exists(path))
            {
                filePath = path;

                DS_LOG_INFO("Loaded Editor Ini file {0}", path);
                if (deleteIniFile)
                {
                    std::filesystem::remove_all(path);
                }
                else
                {
                    ini_file = IniFile(filePath);
                    // ImGui::GetIO().IniFilename = ini[i];

                    fileFound = true;
                    load_editor_settings();
                }
                break;
            }
        }

        if (!fileFound)
        {
            DS_LOG_INFO("Editor Ini not found");
#ifdef DS_PLATFORM_MACOS
            filePath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../editor.ini";
#else
            filePath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "editor.ini";
#endif
            DS_LOG_INFO("Creating Editor Ini {0}", filePath);

            //  FileSystem::WriteTextFile(filePath, "");
            ini_file = IniFile(filePath);
            add_default_editor_settings();
            // ImGui::GetIO().IniFilename = "editor.ini";
        }
#endif

        Application::init();
        Application::set_editor_state(EditorState::Preview);
        Application::get().get_window()->set_event_callback(BIND_EVENT_FN(Editor::handle_event));
#ifdef DS_PLATFORM_WINDOWS
        _putenv_s("PATH", "../Python/");
#endif
        editor_camera = createSharedPtr<Camera>(
            60.0f,
            0.01f,
            settings.camera_far,
            (float)Application::get().get_window_size()[0] / (float)Application::get().get_window_size()[1]);
        current_camera = editor_camera.get();

    /*    glm::mat4 viewMat = glm::inverse(glm::lookAt(glm::vec3(-31.0f, 12.0f, 51.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        editor_camera_transform.set_local_transform(viewMat);*/
        editor_camera_transform.set_local_orientation(glm::radians(glm::vec3(-15.0f, 30.0f, 0.0f)));
        editor_camera_transform.set_world_matrix(glm::mat4(1.0f));
        component_icon_map[typeid(PointLightComponent).hash_code()] = U8CStr2CStr(ICON_MDI_LIGHTBULB);
        component_icon_map[typeid(SpotLightComponent).hash_code()] = U8CStr2CStr(ICON_MDI_LIGHTBULB);
        component_icon_map[typeid(RectLightComponent).hash_code()] = U8CStr2CStr(ICON_MDI_LIGHTBULB);
        component_icon_map[typeid(Camera).hash_code()] = U8CStr2CStr(ICON_MDI_CAMERA);
        component_icon_map[typeid(maths::Transform).hash_code()] = U8CStr2CStr(ICON_MDI_VECTOR_LINE);
        component_icon_map[typeid(GaussianComponent).hash_code()] = U8CStr2CStr(ICON_MDI_VECTOR_POLYGON);
        component_icon_map[typeid(GaussianCrop).hash_code()] = U8CStr2CStr(ICON_MDI_SQUARE);
        component_icon_map[typeid(Editor).hash_code()] = U8CStr2CStr(ICON_MDI_SQUARE);
        component_icon_map[typeid(Environment).hash_code()]    = U8CStr2CStr(ICON_MDI_EARTH);
        //component_icon_map[typeid(TextComponent).hash_code()] = ICON_MDI_TEXT;

        panels.emplace_back(createSharedPtr<ConsolePanel>());
        panels.emplace_back(createSharedPtr<SceneViewPanel>());
        panels.emplace_back(createSharedPtr<InspectorPanel>());
        panels.emplace_back(createSharedPtr<HierarchyPanel>());
        panels.emplace_back(createSharedPtr<KeyFramePanel>());
        cameras_panel = createSharedPtr<ScenePreviewPanel>();
        panels.emplace_back(cameras_panel);
        panels.emplace_back(createSharedPtr<PostProcessPanel>());
        histogram_panel = createSharedPtr<HistogramPanel>(false);
        panels.emplace_back(histogram_panel);
        image_2d_panel = createSharedPtr<Img2DDataSetPanel>(false);
        panels.emplace_back(image_2d_panel);
        //texture_paint_Panel = createSharedPtr<TexturePaintPanel>(false);
        //panels.emplace_back(texture_paint_Panel);
#ifndef DS_PLATFORM_IOS
        //panels.emplace_back(createSharedPtr<ResourcePanel>());
#endif
        helper_panels = createSharedPtr<HelperPanel>();
        helper_panels->set_active(false);
        panels.emplace_back(helper_panels);
        for (auto& panel : panels)
            panel->set_editor(this);

        settings.shiow_imgui_demo = false;

        selected_entities.clear();
        // m_SelectedEntity = entt::null;
        //m_PreviewTexture = nullptr;

        ImGuiHelper::SetTheme(settings.theme);
        OS::instance()->setTitleBarColour(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
        auto version_str = std::to_string(DiverseVersion.major) + "." + std::to_string(DiverseVersion.minor) + "." + std::to_string(DiverseVersion.patch);
        Application::get().get_window()->set_window_title("SplatX v" + version_str);

        ImGuizmo::SetGizmoSizeClipSpace(settings.imguizmo_scale);
        if( std::filesystem::exists(layoutFolder + "dvui.ini"))
            ImGui::LoadIniSettingsFromDisk((layoutFolder + "dvui.ini").c_str());
        ImOGuizmo::config.lineThicknessScale = 0.034f;
        ImOGuizmo::config.positiveRadiusScale = 0.125f;
        ImOGuizmo::config.negativeRadiusScale = 0.1f;
        ImOGuizmo::config.axisLengthScale = 0.4f;
        ImOGuizmo::config.hoverCircleRadiusScale = 1.414f;
        //create pivot
        pivot = std::make_shared<Pivot>(maths::Transform());
        auto project_name = ToStdString(cmdline->option_string(Str8Lit("open_project")));
        if( !project_name.empty())
            project_open_callback(project_name);
    }

    bool Editor::handle_file_drop(WindowFileEvent& e)
    {
        const auto file_paths = e.GetFilePaths();
        for (auto f : file_paths) {
            dropped_files.push_back(utf8_to_multibyte(f));
        }
        if (file_paths.size() == 1)
        {
            file_open_callback(utf8_to_multibyte(file_paths[0]));
        }
        else
        {
            std::vector<std::string>    img_datas;
            for (const auto& p : file_paths)
            {
                auto pf = utf8_to_multibyte(p);
                if(is_image_file(pf))
                {
                    img_datas.emplace_back(pf);
                }
            }
            if( img_datas.size() > 1)
            {
                is_open_newgaussian_popup = true;
                splat_source_path = std::move(img_datas);
            }
        }
        for(auto panel : panels)
            panel->handle_file_drop(e);
        return true;
    }
    
    bool Editor::handle_mouse_move(MouseMovedEvent& e)
	{     
        for(auto panel : panels)
            panel->handle_mouse_move(e);
		return true;
	}
    bool Editor::handle_mouse_pressed(MouseButtonPressedEvent& e)
	{
	    for(auto panel : panels)
            panel->handle_mouse_pressed(e);
		return true;
	}
    bool Editor::handle_mouse_released(MouseButtonReleasedEvent& e)
	{
		for(auto panel : panels)
            panel->handle_mouse_released(e);
		return true;
	}
    
    void Editor::handle_new_scene(Scene* scene)
    {
        DS_PROFILE_FUNCTION();
        Application::handle_new_scene(scene);
        // m_SelectedEntity = entt::null;
        selected_entities.clear();
        auto box = scene->get_world_bounding_box();
        focus_camera(box.center(), 2.0f, 2.0f);
        for (auto panel : panels)
        {
            panel->on_new_scene(scene);
        }
    }

    bool Editor::is_editing_splat()
    {
        auto& reg = get_current_scene()->get_registry();
        auto select_valid = reg.valid(get_current_splat_entt());
        if ( select_valid)
        { 
            auto editType = GaussianEdit::get().get_edit_type();
            auto ret = editType != diverse::GaussianEdit::EditType::None;
            return ret;
        }
        return false;
    }

    bool Editor::is_gaussian_file(const std::string& filePath)
    {
        DS_PROFILE_FUNCTION();
        return MeshModel::is_gaussian_file(filePath);
    }

    bool Editor::is_image_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        if (extension == "png" || extension == "jpg" || extension == "tga" || extension == "hdr" || extension == "exr")
            return true;

        return false;
    }
    bool Editor::is_hdr_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        if (extension == "hdr" || extension == "exr")
            return true;

        return false;
    }
    
    bool Editor::is_video_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        if (extension == "mp4" || extension == "avi" || extension == "m4v" || extension == "mov")
            return true;

        return false;
    }

    bool Editor::is_project_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        if (extension == "dvs")
            return true;
        return false;
    }

    bool Editor::is_mesh_model_file(const std::string& filePath)
    {
        return MeshModel::is_mesh_model_file(filePath);
    }

    bool Editor::is_text_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);

        if (extension == "txt" || extension == "glsl" || extension == "shader" || extension == "vert"
            || extension == "frag" || extension == "lua" || extension == "Lua")
            return true;
        return false;
    }

    bool Editor::is_audio_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        if (extension == "ogg" || extension == "wav")
            return true;
        return false;
    }

    bool Editor::is_scene_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        if (extension == "dsn")
            return true;
        return false;
    }
    bool Editor::is_font_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        if (extension == "ttf")
            return true;
        return false;
    }

    bool Editor::is_shader_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        if (extension == "hlsl")
            return true;
        return false;
    }

    bool Editor::is_camera_json_file(const std::string& filePath)
    {
        std::string extension = stringutility::get_file_extension(filePath);
        if (extension == "json")
            return true;
        return false;
    }

    bool Editor::is_texture_paint_active() const
    {
        if(!texture_paint_Panel) return false;
        return texture_paint_Panel->active();
    }

    void Editor::open_file()
    {
        auto path = OS::instance()->getExecutablePath();
        std::filesystem::current_path(path);
        file_browser_panel.set_callback(BIND_FILEBROWSER_FN(Editor::file_open_callback));
        file_browser_panel.open();
    }

    const char* Editor::get_iconfont_icon(const std::string& filePath)
    {
        DS_PROFILE_FUNCTION();
        if (is_text_file(filePath))
        {
            return U8CStr2CStr(ICON_MDI_FILE_XML);
        }
        else if (is_mesh_model_file(filePath) || is_gaussian_file(filePath))
        {
            return U8CStr2CStr(ICON_MDI_SHAPE);
        }
        else if (is_audio_file(filePath))
        {
            return U8CStr2CStr(ICON_MDI_FILE_MUSIC);
        }
        else if (is_image_file(filePath))
        {
            return U8CStr2CStr(ICON_MDI_FILE_IMAGE);
        }

        return U8CStr2CStr(ICON_MDI_FILE);
    }

    std::shared_ptr<asset::Texture> Editor::get_current_train_view_image()
    {
        auto  image_panel = dynamic_cast<Img2DDataSetPanel*>(image_2d_panel.get());
        if (image_panel)
        {
			return image_panel->get_current_train_view_texture();
		}
        return nullptr;
    }

    std::shared_ptr<asset::Texture> Editor::get_hovered_train_view()
    {
        auto  image_panel = dynamic_cast<Img2DDataSetPanel*>(image_2d_panel.get());
        if (image_panel)
        {
            return image_panel->get_train_view_texture(hovered_train_view_id);
        }
        return nullptr;
    }

    void Editor::file_open_callback(const std::string& filePath)
    {
        DS_PROFILE_FUNCTION();
        auto path = filePath;
        path = stringutility::back_slashes_2_slashes(path);
        if (std::filesystem::is_directory(filePath))
        {
            is_open_newgaussian_popup = true;
            splat_source_path = { filePath };
        }
        else if (is_gaussian_file(filePath) || is_mesh_model_file(filePath))
        {
            load_model_path = filePath;
            is_load_model_popup = true;
            is_load_mesh = is_mesh_model_file(filePath);
        }
        else if (PointCloud::is_point_cloud_file(filePath))
        {
            load_model_path = filePath;
            is_load_model_popup = true;
            is_load_point = true;
        }
        else if( is_video_file(path))
        {
            is_open_newgaussian_popup = true;
            splat_source_path = {filePath};
        }
        else if (is_audio_file(path))
        {

        }
        else if(is_scene_file(path))
        {
            int index = Application::get().get_scene_manager()->enqueue_scene_from_file(path);
            Application::get().get_scene_manager()->switch_scene(index);
        }
        else if( is_project_file(path))
        {
            project_open_callback(path);
        }
    }

    void Editor::project_open_callback(const std::string& filePath)
    {
        is_new_project_popup_open = false;
        reopenNewProjectPopup = false;
        locationPopupOpened = false;

        if (FileSystem::file_exists(filePath))
        {
            auto it = std::find(settings.recent_projects.begin(), settings.recent_projects.end(), filePath);
            if (it == settings.recent_projects.end())
                settings.recent_projects.push_back(filePath);
        }
      /*  System::JobSystem::Context context;
        System::JobSystem::execute(context, [this, filePath](JobDispatchArgs args) {*/
            Application::get().open_project(filePath);

            for (int i = 0; i < int(panels.size()); i++)
            {
                panels[i]->on_new_project();
            }
        //});
    }

    void Editor::new_project_open_callback(const std::string& filePath)
    {
        Application::get().open_new_project(filePath);

        for (int i = 0; i < int(panels.size()); i++)
        {
            panels[i]->on_new_project();
        }
    }


    void Editor::add_default_editor_settings()
    {
        DS_PROFILE_FUNCTION();
        DS_LOG_INFO("Setting default editor settings");
        project_settings.ProjectRoot = "../../ExampleProject/";
        project_settings.ProjectName = "Example";

        ini_file.Add("ShowGrid", settings.show_grid);
        ini_file.Add("ShowGizmos", settings.show_gizmos);
        ini_file.Add("ShowViewSelected", settings.show_view_selected);
        ini_file.Add("TransitioningCamera", is_transitioning_camera);
        ini_file.Add("ShowImGuiDemo", settings.shiow_imgui_demo);
        ini_file.Add("SnapAmount", settings.snap_amount);
        ini_file.Add("SnapQuizmo", settings.snap_quizmo);
        ini_file.Add("DebugDrawFlags", settings.debug_draw_flags);
        ini_file.Add("Theme", (int)settings.theme);
        ini_file.Add("ProjectRoot", project_settings.ProjectRoot);
        ini_file.Add("ProjectName", project_settings.ProjectName);
        ini_file.Add("SleepOutofFocus", settings.sleep_outof_focus);
        ini_file.Add("RecentProjectCount", 0);
        ini_file.Add("CameraSpeed", settings.camera_speed);
        ini_file.Add("CameraNear", settings.camera_near);
        ini_file.Add("CameraFar", settings.camera_far);
        ini_file.Add("saveSceneShowAgain", saveSceneShowAgain);
        ini_file.SetOrAdd("TrainGaussian", is_train_gaussian);
        ini_file.SetOrAdd("SplatUpdateFreq", splat_update_freq);
        ini_file.Rewrite();
    }

    void Editor::save_editor_settings()
    {
        DS_PROFILE_FUNCTION();
        ini_file.RemoveAll();
        ini_file.SetOrAdd("ShowGrid", settings.show_grid);
        ini_file.SetOrAdd("ShowGizmos", settings.show_gizmos);
        ini_file.SetOrAdd("ShowViewSelected", settings.show_view_selected);
        ini_file.SetOrAdd("ShowImGuiDemo", settings.shiow_imgui_demo);
        ini_file.SetOrAdd("SnapAmount", settings.snap_amount);
        ini_file.SetOrAdd("SnapQuizmo", settings.snap_quizmo);
        ini_file.SetOrAdd("DebugDrawFlags", settings.debug_draw_flags);
        ini_file.SetOrAdd("Theme", (int)settings.theme);
        ini_file.SetOrAdd("ProjectRoot", project_settings.ProjectRoot);
        ini_file.SetOrAdd("ProjectName", project_settings.ProjectName);
        ini_file.SetOrAdd("SleepOutofFocus", settings.sleep_outof_focus);
        ini_file.SetOrAdd("CameraSpeed", settings.camera_speed);
        ini_file.SetOrAdd("CameraNear", settings.camera_near);
        ini_file.SetOrAdd("CameraFar", settings.camera_far);

        std::sort(settings.recent_projects.begin(), settings.recent_projects.end());
        settings.recent_projects.erase(std::unique(settings.recent_projects.begin(), settings.recent_projects.end()), settings.recent_projects.end());
        ini_file.SetOrAdd("RecentProjectCount", int(settings.recent_projects.size()));

        for (int i = 0; i < int(settings.recent_projects.size()); i++)
        {
            ini_file.SetOrAdd("RecentProject" + std::to_string(i), settings.recent_projects[i]);
        }
        ini_file.SetOrAdd("saveSceneShowAgain", saveSceneShowAgain);
        ini_file.SetOrAdd("TrainGaussian", is_train_gaussian);
        ini_file.SetOrAdd("SplatUpdateFreq", splat_update_freq);
        ini_file.Rewrite();
    }

    void Editor::load_editor_settings()
    {
        DS_PROFILE_FUNCTION();
        settings.show_grid = ini_file.GetOrDefault("ShowGrid", settings.show_grid);
        settings.show_gizmos = ini_file.GetOrDefault("ShowGizmos", settings.show_gizmos);
        settings.show_view_selected = ini_file.GetOrDefault("ShowViewSelected", settings.show_view_selected);
        is_transitioning_camera = ini_file.GetOrDefault("TransitioningCamera", is_transitioning_camera);
        settings.shiow_imgui_demo = ini_file.GetOrDefault("ShowImGuiDemo", settings.shiow_imgui_demo);
        settings.snap_amount = ini_file.GetOrDefault("SnapAmount", settings.snap_amount);
        settings.snap_quizmo = ini_file.GetOrDefault("SnapQuizmo", settings.snap_quizmo);
        settings.debug_draw_flags = ini_file.GetOrDefault("DebugDrawFlags", settings.debug_draw_flags);
        settings.theme = ImGuiHelper::Theme(ini_file.GetOrDefault("Theme", (int)settings.theme));

        project_settings.ProjectRoot = ini_file.GetOrDefault("ProjectRoot", std::string("../../ExampleProject/"));
        project_settings.ProjectName = ini_file.GetOrDefault("ProjectName", std::string("Example"));
        settings.sleep_outof_focus = ini_file.GetOrDefault("SleepOutofFocus", true);
        settings.camera_speed = ini_file.GetOrDefault("CameraSpeed", 100.0f);
        settings.camera_near = ini_file.GetOrDefault("CameraNear", 0.01f);
        settings.camera_far = ini_file.GetOrDefault("CameraFar", 1000.0f);

        editor_camera_controller.set_speed(settings.camera_speed);

        int recentProjectCount = 0;
        std::string projectPath = project_settings.ProjectRoot + project_settings.ProjectName + std::string(".dvs");

        if (FileSystem::file_exists(projectPath))
        {
            auto it = std::find(settings.recent_projects.begin(), settings.recent_projects.end(), projectPath);
            if (it == settings.recent_projects.end())
                settings.recent_projects.push_back(projectPath);
        }

        recentProjectCount = ini_file.GetOrDefault("RecentProjectCount", 0);
        for (int i = 0; i < recentProjectCount; i++)
        {
            projectPath = ini_file.GetOrDefault("RecentProject" + std::to_string(i), std::string());

            if (FileSystem::folder_exists(projectPath))
            {
                auto it = std::find(settings.recent_projects.begin(), settings.recent_projects.end(), projectPath);
                if (it == settings.recent_projects.end())
                    settings.recent_projects.push_back(projectPath);
            }
        }
        saveSceneShowAgain = ini_file.GetOrDefault("saveSceneShowAgain", saveSceneShowAgain);
        is_train_gaussian = ini_file.GetOrDefault("TrainGaussian", is_train_gaussian);
        splat_update_freq = ini_file.GetOrDefault("SplatUpdateFreq", splat_update_freq);
        std::sort(settings.recent_projects.begin(), settings.recent_projects.end());
        settings.recent_projects.erase(std::unique(settings.recent_projects.begin(), settings.recent_projects.end()), settings.recent_projects.end());
    }

    maths::Ray Editor::get_screen_ray(int x, int y, Camera* camera, int width, int height)
    {
        DS_PROFILE_FUNCTION();
        if (!camera)
            return maths::Ray();

        float screenX = (float)x / (float)width;
        float screenY = (float)y / (float)height;

        bool flipY = true;

#ifdef DS_RENDER_API_OPENGL
        if (get_render_api() == RenderAPI::OPENGL)
            flipY = true;
#endif
        return camera->get_screen_ray(screenX, screenY, glm::inverse(editor_camera_transform.get_world_matrix()), flipY);
    }

    void Editor::draw_2dgrid(ImDrawList* drawList, 
                            const ImVec2& cameraPos, 
                            const ImVec2& windowPos, 
                            const ImVec2& canvasSize, 
                            const float factor, 
                            const float thickness)
    {
        DS_PROFILE_FUNCTION();
        static const auto graduation = 10;
        float GRID_SZ = canvasSize.y * 0.5f / factor;
        const ImVec2& offset = {
            canvasSize.x * 0.5f - cameraPos.x * GRID_SZ, canvasSize.y * 0.5f + cameraPos.y * GRID_SZ
        };

        ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
        float gridThickness = 1.0f;

        const auto& gridColor = GRID_COLOR;
        auto smallGraduation = GRID_SZ / graduation;
        const auto& smallGridColor = IM_COL32(100, 100, 100, smallGraduation);

        for (float x = -GRID_SZ; x < canvasSize.x + GRID_SZ; x += GRID_SZ)
        {
            auto localX = floorf(x + fmodf(offset.x, GRID_SZ));
            drawList->AddLine(
                ImVec2{ localX, 0.0f } + windowPos, ImVec2{ localX, canvasSize.y } + windowPos, gridColor, gridThickness);

            if (smallGraduation > 5.0f)
            {
                for (int i = 1; i < graduation; ++i)
                {
                    const auto graduation = floorf(localX + smallGraduation * i);
                    drawList->AddLine(ImVec2{ graduation, 0.0f } + windowPos,
                        ImVec2{ graduation, canvasSize.y } + windowPos,
                        smallGridColor,
                        1.0f);
                }
            }
        }

        for (float y = -GRID_SZ; y < canvasSize.y + GRID_SZ; y += GRID_SZ)
        {
            auto localY = floorf(y + fmodf(offset.y, GRID_SZ));
            drawList->AddLine(
                ImVec2{ 0.0f, localY } + windowPos, ImVec2{ canvasSize.x, localY } + windowPos, gridColor, gridThickness);

            if (smallGraduation > 5.0f)
            {
                for (int i = 1; i < graduation; ++i)
                {
                    const auto graduation = floorf(localY + smallGraduation * i);
                    drawList->AddLine(ImVec2{ 0.0f, graduation } + windowPos,
                        ImVec2{ canvasSize.x, graduation } + windowPos,
                        smallGridColor,
                        1.0f);
                }
            }
        }
    }

    void Editor::draw_3dgrid()
    {
        DS_PROFILE_FUNCTION();
#if 1
        if ( !Application::get().get_scene_manager()->get_current_scene())
        {
            return;
        }

        DebugRenderer::DrawHairLine(glm::vec3(-5000.0f, 0.0f, 0.0f), glm::vec3(5000.0f, 0.0f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        DebugRenderer::DrawHairLine(glm::vec3(0.0f, -5000.0f, 0.0f), glm::vec3(0.0f, 5000.0f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        DebugRenderer::DrawHairLine(glm::vec3(0.0f, 0.0f, -5000.0f), glm::vec3(0.0f, 0.0f, 5000.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
        
        //m_Renderer->draw_3dgrid();
#endif
    }

    void Editor::select_object(const maths::Ray& ray,bool hoveredOnly)
    {
        DS_PROFILE_FUNCTION();
        auto& registry = Application::get().get_scene_manager()->get_current_scene()->get_registry();
        float closestEntityDist = maths::M_INFINITY;
        entt::entity currentClosestEntity = entt::null;

        auto group = registry.group<GaussianComponent>(entt::get<maths::Transform>);

        static Timer timer;
        static float timeSinceLastSelect = 0.0f;
#ifdef DS_SPLAT_TRAIN
        //gaussian train scene
        {
            auto gsGroup = registry.group<GaussianTrainerScene>(entt::get<maths::Transform>);
            float closet_t = maths::M_INFINITY;
            for (auto entity : gsGroup)
            {
                const auto& [gstrain, transform] = gsGroup.get<GaussianTrainerScene, maths::Transform>(entity);
                auto cur_status = gstrain.getCurrentTrainingStatus();
                if (gstrain.ShowTrainView && cur_status == TrainingStatus::Training)
                {
                    auto model = registry.get<GaussianComponent>(entity).ModelRef;
                    for(auto i=0;i < gstrain.getNumCameras();i++)
                    {
                        auto projection = gstrain.getCameraProjection(i);
                        //projection[1][1] *= -1;
                        auto viewR = gstrain.getCameraRotation(i);
                        auto viewT = gstrain.getCameraPos(i);
                        auto view = glm::translate(glm::identity<glm::mat4>(), viewT) * glm::mat4_cast(viewR);
                        maths::Frustum cameraFrustum(projection, glm::inverse(view) * glm::inverse(transform.get_world_matrix()));
                        float distance;
                        ray.intersects(cameraFrustum.get_bounding_box(), distance);
                        if(distance < closet_t)
                        {
                            if(!hoveredOnly)
                                current_train_view_id = i;
                            else
                                hovered_train_view_id = i;
                            closet_t = distance;
                            break;
                        }
                    }
                }
            }
            if( closet_t == closestEntityDist)
			{
                if(!hoveredOnly)
                    current_train_view_id = -1;
                else
                    hovered_train_view_id = -1;
			}
        }
#endif
        for (auto entity : group)
        {
            if( !Entity(entity,get_current_scene()).active() ) continue;
            const auto& [model, trans] = group.get<GaussianComponent, maths::Transform>(entity);

            auto& worldTransform = trans.get_world_matrix();
            auto bbCopy = model.ModelRef->get_world_bounding_box(worldTransform);
            float distance;
            ray.intersects(bbCopy, distance);

            if (distance < maths::M_INFINITY)
            {
                if (distance < closestEntityDist)
                {
                    closestEntityDist = distance;
                    currentClosestEntity = entity;
                }
            }
        }

        //mesh model
        auto mesh_group = registry.group<MeshModelComponent>(entt::get<maths::Transform>);
        for (auto entity : mesh_group)
        {
            if( !Entity(entity,get_current_scene()).active() ) continue;
            const auto& [model, trans] = mesh_group.get<MeshModelComponent, maths::Transform>(entity);
            auto& worldTransform = trans.get_world_matrix();
            const auto& meshes = model.ModelRef->get_meshes();
            for (auto mesh : meshes)
            {
                auto bbCopy = mesh->get_bounding_box().transformed(worldTransform);
                float distance;
                ray.intersects(bbCopy, distance);

                if (distance < maths::M_INFINITY)
                {
                    if (distance < closestEntityDist)
                    {
                        closestEntityDist = distance;
                        currentClosestEntity = entity;
                    }
                }
            }
        }
        if (!hoveredOnly)
        {
            if (!selected_entities.empty())
            {
                if (registry.valid(currentClosestEntity) && is_selected(currentClosestEntity))
                {
                    if (timer.GetElapsedS() - timeSinceLastSelect < 1.0f)
                    {
                        // auto& trans = registry.get<maths::Transform>(currentClosestEntity);
                        auto& pivot_transform = get_pivot()->get_transform();
                        if(!is_editing_splat())
                            focus_camera(pivot_transform.get_world_position(), 2.0f, 2.0f);
                    }
                    else
                    {
                        un_select(currentClosestEntity);
                    }
                }

                timeSinceLastSelect = timer.GetElapsedS();

                auto& io = ImGui::GetIO();
                auto ctrl = Input::get().get_key_held(InputCode::Key::LeftSuper) || (Input::get().get_key_held(InputCode::Key::LeftControl));

                if (!ctrl)
                    selected_entities.clear();

                set_selected(currentClosestEntity);
                return;
            }
        }
        if (hoveredOnly)
            set_hovered_entity(currentClosestEntity);
        else
            set_selected(currentClosestEntity);
    }
    void Editor::focus_camera(const glm::vec3& point, float distance, float speed)
    {
        DS_PROFILE_FUNCTION();

        editor_camera_controller.stop_movement();

        if (current_camera->is_orthographic())
        {
            editor_camera_transform.set_local_position(point);
        }
        else
        {
            is_transitioning_camera = true;

            camera_destination = point + editor_camera_transform.get_forward_direction() * distance;
            camera_transition_speed = speed;
        }
    }

    void Editor::imgui_render()
    {
        DS_PROFILE_FUNCTION();
        draw_menu_bar();

        begin_dock_space(Application::get().get_editor_state() == EditorState::Play);
        for (auto& panel : panels)
        {
            if (panel->active())
                panel->on_imgui_render();
        }
        file_browser_panel.on_imgui_render();

        auto& io = ImGui::GetIO();
        //auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
        //if (ctrl && Input::get().get_key_pressed(diverse::InputCode::Key::P))
        //{
        //    show_command_palette = !show_command_palette;
        //}
        //if (show_command_palette)
        //{
        //    ImCmd::CommandPaletteWindow("CommandPalette", &show_command_palette);
        //}

        if (Application::get().get_editor_state() == EditorState::Preview)
        {
            Application::get().get_scene_manager()->get_current_scene()->update_scene_graph();
        }
        end_dock_space();
        ImGuiDockContext* dc = &ImGui::GetCurrentContext()->DockContext;
        for (int n = 0; n < dc->Nodes.Data.Size; n++)
            if (ImGuiDockNode* node = (ImGuiDockNode*)dc->Nodes.Data[n].val_p)
            {
                if (node->TabBar)
                    for (int n = 0; n < node->TabBar->Tabs.Size; n++)
                    {
                        const bool tab_visible = node->TabBar->VisibleTabId == node->TabBar->Tabs[n].ID;
                        const bool tab_bar_focused = (node->TabBar->Flags & ImGuiTabBarFlags_IsFocused) != 0;
                        if (tab_bar_focused || tab_visible)
                        {
                            auto tab = node->TabBar->Tabs[n];

                            ImVec2 pos = tab.Window->Pos;
                            pos.x = pos.x + tab.Offset + ImGui::GetStyle().FramePadding.x;
                            pos.y = pos.y + ImGui::GetStyle().ItemSpacing.y;
                            ImRect bb(pos, { pos.x + tab.Width, pos.y });

                            tab.Window->DrawList->AddLine(bb.Min, bb.Max, (!tab_bar_focused) ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive) : ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
                        }
                    }
            }

        Application::imgui_render();
    }

    void Editor::render()
    {
        DS_PROFILE_FUNCTION();
        // DrawPreview();

        bool isProfiling = false;
        static bool firstFrame = true;
#if DS_PROFILE
        isProfiling = tracy::GetProfiler().IsConnected();
#endif
        if (!isProfiling && settings.sleep_outof_focus && !Application::get().get_window()->get_window_focus() && editor_state != EditorState::Preview && !firstFrame)
            OS::instance()->delay(1000000);

        Application::render();

        for (int i = 0; i < int(panels.size()); i++)
        {
            panels[i]->on_render();
        }

        if (settings.show_grid && !editor_camera->is_orthographic())
            draw_3dgrid();

        firstFrame = false;
    }


    void Editor::debug_draw()
    {
        Application::debug_draw();
        DS_PROFILE_FUNCTION();
        auto& registry = Application::get().get_scene_manager()->get_current_scene()->get_registry();
        glm::vec4 selectedColour = glm::vec4(0.9f);
     /*   if (settings.debug_draw_flags & EditorDebugFlags::MeshBoundingBoxes)
        {
            auto group = registry.group<GaussianComponent>(entt::get<maths::Transform>);

            for (auto entity : group)
            {
                const auto& [model, trans] = group.get<GaussianComponent, maths::Transform>(entity);
                auto& worldTransform = trans.get_world_matrix();
                auto bbCopy = model.ModelRef->boundingBox.transformed(worldTransform);
                DebugRenderer::DebugDraw(bbCopy, selectedColour, true);
            }
        }*/

        //if (settings.debug_draw_flags & EditorDebugFlags::CameraFrustum)
#ifdef DS_SPLAT_TRAIN
        //gaussian train scene
        {
            auto gsGroup = registry.group<GaussianTrainerScene>(entt::get<maths::Transform>);

            for (auto entity : gsGroup)
            {
                const auto& [gstrain, transform] = gsGroup.get<GaussianTrainerScene, maths::Transform>(entity);
                if (gstrain.ShowTrainView && gstrain.getCurrentTrainingStatus() == TrainingStatus::Training)
                {
                    auto model = registry.get<GaussianComponent>(entity).ModelRef;
                    for(auto i=0;i < gstrain.getNumCameras();i++)
                    {
                        auto projection = gstrain.getCameraProjection(i);
                        //projection[1][1] *= -1;
                        auto viewR = gstrain.getCameraRotation(i);
                        auto viewT = gstrain.getCameraPos(i);
                        auto view = glm::translate(glm::identity<glm::mat4>(), viewT) * glm::mat4_cast(viewR);
                        maths::Frustum cameraFrustum(projection, glm::inverse(view) * glm::inverse(transform.get_world_matrix()));
                        DebugRenderer::DebugDraw(cameraFrustum, current_train_view_id == i ? glm::vec4(0.8,0,0,0.9): glm::vec4(0.9f));
                    }
                }
            }
        }
#endif

        if (settings.debug_draw_flags & EditorDebugFlags::MeshBoundingBoxes)
        { 
            for (auto select_ent : selected_entities)
                if (registry.valid(select_ent)) // && Application::Get().GetEditorState() == EditorState::Preview)
                {
                    auto transform = registry.try_get<maths::Transform>(select_ent);
                    auto point_light = registry.try_get<PointLightComponent>(select_ent);
                    if (point_light && transform)
                    {
                        DebugRenderer::DebugDraw(point_light, *transform, glm::vec4(glm::vec3(point_light->get_radiance()), 0.2f));
                    }

                    auto spot_light = registry.try_get<SpotLightComponent>(select_ent);
                    if (spot_light && transform)
                    {
                        DebugRenderer::DebugDraw(spot_light, *transform, glm::vec4(glm::vec3(spot_light->get_radiance()), 0.2f));
                    }

                    auto rect_light = registry.try_get<RectLightComponent>(select_ent);
                    if (rect_light && transform)
                    {
                        DebugRenderer::DebugDraw(rect_light, *transform, glm::vec4(glm::vec3(rect_light->get_radiance()), 0.2f));
                    }

                    #define drawdebugBox(T) {                                               \
                        auto model = registry.try_get<T>(select_ent); \
                        if (transform && model && model->ModelRef && model->ModelRef->is_flag_set(AssetFlag::Loaded))                          \
                        {                                                                   \
                            auto& worldTransform = transform->get_world_matrix();           \
                            auto bbCopy = model->ModelRef->get_world_bounding_box(worldTransform);\
                            DebugRenderer::DebugDraw(bbCopy, selectedColour, true);         \
                        }                                                                   \
                    };                                                                      
                    drawdebugBox(GaussianComponent)
                    drawdebugBox(MeshModelComponent)
                    drawdebugBox(PointCloudComponent)
                }
        }
    }
    void Editor::update(const TimeStep& ts)
    {
        DS_PROFILE_FUNCTION();
        if(get_current_scene()->serialised())
            saveScenePopup = false;
        static float autoSaveTimer = 0.0f;
        if(auto_save_settings_time > 0)
        {
            if(autoSaveTimer > auto_save_settings_time)
            {
                save_editor_settings();
                if (get_current_scene()->serialised())  //only when scene has been rename serialized, we can save again
                    get_current_scene()->serialise(project_settings.ProjectRoot + "assets/scenes/", false);
                autoSaveTimer = 0;
            }

            autoSaveTimer += (float)ts.get_millis();
        }
        auto& registry = get_current_scene()->get_entity_manager()->get_registry();
        selected_entities.erase(std::remove_if(selected_entities.begin(), selected_entities.end(), [&registry](entt::entity entity)
            { return !registry.valid(entity); }),
            selected_entities.end());
        if(editor_state == EditorState::Play)
            autoSaveTimer = 0.0f;
        //add default Environment
        auto scene = get_current_scene();
        if (scene)
        {
            auto environments = scene->get_entity_manager()->get_entities_with_type<Environment>();
            if (environments.size() == 0)
            {
                auto environment = scene->get_entity_manager()->create("Environment");
                environment.add_component<Environment>();
                environment.get_component<Environment>().load();
            }
        }
        if (gs2mesh_load)
        {
            Entity modelEntity = scene->get_entity_manager()->create(stringutility::get_file_name(load_model_path));
            modelEntity.add_component<MeshModelComponent>(load_model_path);
            set_selected(modelEntity.get_handle());
            auto& trans = modelEntity.get_component<maths::Transform>();
            focus_camera(trans.get_world_position(), 2.0f, 2.0f);
            auto group = scene->get_entity_manager()->get_entities_with_type<Environment>();
            for (auto ent : group)
            {
                auto& enviroment = ent.get_component<Environment>();
                if (enviroment.mode == Environment::Mode::Pure)
                {
                    enviroment.mode = Environment::Mode::SunSky;
                    auto& sun_controller = ent.get_or_add_component<diverse::SunController>();
                    sun_controller.calculate_towards_sun_from_theta_phi(glm::radians(enviroment.theta), glm::radians(enviroment.phi));
                }
            }
            gs2mesh_load = false;
        }
        if (Input::get().get_key_pressed(diverse::InputCode::Key::Escape) && get_editor_state() != EditorState::Preview)
        {

            Application::get().set_editor_state(EditorState::Preview);

            selected_entities.clear();
            ImGui::SetWindowFocus("###scene");
            //load cache scene
            set_editor_state(EditorState::Preview);
        }
        if (scene_view_active)
        {
            auto& registry = scene->get_registry();

            // if(Application::Get().GetSceneActive())
            {
                const glm::vec2 mousePos = Input::get().get_mouse_position();
                editor_camera_controller.set_camera(editor_camera.get());

                // Make sure the camera is not controllable during transitions
                if (!is_transitioning_camera)
                {
                    editor_camera_controller.handle_mouse(editor_camera_transform, (float)ts.get_seconds(), mousePos.x, mousePos.y);
                    editor_camera_controller.handle_keyboard(editor_camera_transform, (float)ts.get_seconds());
                }

                editor_camera_transform.set_world_matrix(glm::mat4(1.0f));

                if (!selected_entities.empty() && Input::get().get_key_pressed(InputCode::Key::F))
                {
                    if (registry.valid(selected_entities.front()))
                    {
                        auto transform = registry.try_get<maths::Transform>(selected_entities.front());
                        if (transform)
                            focus_camera(transform->get_world_position(), 2.0f, 2.0f);
                    }
                }
            }

            if (Input::get().get_key_held(InputCode::Key::O))
            {
                focus_camera(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f, 2.0f);
            }
            if (Input::get().get_key_pressed(InputCode::Key::H))
            {
                helper_panels->set_active(!helper_panels->active());
            }
            if (Input::get().get_key_pressed(InputCode::Key::Space))
                g_render_settings.render_mode = RenderMode::PT == g_render_settings.render_mode ? RenderMode::Hybrid : RenderMode::PT;

            if (is_transitioning_camera)
            {
                // Defines the tolerance for distance, beyond which a transition is considered completed
                constexpr float kTransitionCompletionDistanceTolerance = 0.01f;
                constexpr float kSpeedBaseFactor = 5.0f;

                const auto cameraCurrentPosition = editor_camera_transform.get_local_position();

                editor_camera_controller.update_focal_point(editor_camera_transform, glm::mix(
                    cameraCurrentPosition,
                    camera_destination,
                    glm::clamp(camera_transition_speed * kSpeedBaseFactor * static_cast<float>(ts.get_seconds()), 0.0f, 1.0f)
                ));
                auto distanceToDestination = glm::distance(cameraCurrentPosition, camera_destination);

                is_transitioning_camera = distanceToDestination > kTransitionCompletionDistanceTolerance;
            }

            if(!Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight) && !ImGuizmo::IsUsing())
            {
                if(Input::get().get_key_pressed(InputCode::Key::Q))
                {
                    set_imguizmo_operation(ImGuizmo::OPERATION::BOUNDS);
                }

                if(Input::get().get_key_pressed(InputCode::Key::T))
                {
                    set_imguizmo_operation(ImGuizmo::OPERATION::TRANSLATE);
                }

                if(Input::get().get_key_pressed(InputCode::Key::R))
                {
                    set_imguizmo_operation(ImGuizmo::OPERATION::ROTATE);
                }

                if(Input::get().get_key_pressed(InputCode::Key::S))
                {
                    set_imguizmo_operation(ImGuizmo::OPERATION::SCALE);
                }

                if(Input::get().get_key_pressed(InputCode::Key::U))
                {
                    set_imguizmo_operation(ImGuizmo::OPERATION::UNIVERSAL);
                }

                if(Input::get().get_key_pressed(InputCode::Key::Y))
                {
                    toggle_snap();
                }
            }

            auto& splatEdit = GaussianEdit::get();
            if (Input::get().get_key_pressed(InputCode::Key::Delete) && !selected_entities.empty())
            {
                for (auto entity : selected_entities)
                {
                    auto cur_ent = Entity(entity, get_current_scene());
                    auto editType = splatEdit.get_edit_type();
                    auto ret = editType == diverse::GaussianEdit::EditType::None;
                    if( !splatEdit.has_select_gaussians() && ret)
                    {
                        if (entity == current_train_entity) 
                            current_train_entity = entt::null;
                        cur_ent.destroy();
                        if (entity == current_splat_entity)
                            splatEdit.set_edit_splat(nullptr);
                    }
                }
            }
            if((Input::get().get_key_held(InputCode::Key::LeftSuper) || (Input::get().get_key_held(InputCode::Key::LeftControl))))
            {
                if(Input::get().get_key_pressed(InputCode::Key::S) && Application::get().get_scene_active())
                {
                    if (get_current_scene()->serialised())  //only when scene has been rename serialized, we can save again
                        get_current_scene()->serialise(project_settings.ProjectRoot + "assets/scenes/", false);
                    else
                        saveScenePopup = true;
                }

                if( Input::get().get_key_pressed(InputCode::Key::N))
                    newScenePopup = true;
                if (Input::get().get_key_pressed(InputCode::Key::G))
                    is_open_newgaussian_popup = true;
                //reload scene
                if(Input::get().get_key_pressed(InputCode::Key::O))
                    get_current_scene()->deserialise(project_settings.ProjectRoot + "assets/scenes/", false);
                if(Input::get().get_key_pressed(InputCode::Key::X))
                {
                    if (splatEdit.has_select_gaussians())
                    {
                        splatEdit.add_seperate_selection_op();
                    }
                    else
                    { 
                        for(auto entity : selected_entities)
                            set_copied_entity(entity, true);
                    }
                }
                
                if(Input::get().get_key_pressed(InputCode::Key::C))
                {
                    if (splatEdit.has_select_gaussians())
                    {
                        splatEdit.add_duplicate_selection_2_instance_op();
                    }
                    else
                        for(auto entity : selected_entities)
                            set_copied_entity(entity, false);
                }

                if(Input::get().get_key_pressed(InputCode::Key::V) && !copied_entities.empty())
                {
                    for(auto entity : copied_entities)
                    {
                        get_current_scene()->duplicate_entity({ entity, get_current_scene() });
                        if(cut_copy_entity)
                        {
                            Entity(entity, get_current_scene()).destroy();
                            if(entity == current_splat_entity)
                                splatEdit.set_edit_splat(nullptr);
                            if (entity == current_train_entity)
                                current_train_entity = entt::null;
                        }
                    }
                }

                if(Input::get().get_key_pressed(InputCode::Key::D) && !selected_entities.empty())
                {
                    if(splatEdit.has_select_gaussians())
                    { 
                        splatEdit.add_duplicate_selection_op();
                    }
                    else
                    { 
                        for(auto entity : copied_entities)
                            get_current_scene()->duplicate_entity({ entity, get_current_scene() });
                    }
                }
                if (Input::get().get_key_pressed(InputCode::Key::Z))
                {
                    UndoRedoSystem::get().undo();
                }
            }
        }
        else
        {
            editor_camera_controller.stop_movement();
        }

        update_gaussian(ts);
        for (int i = 0; i < int(panels.size()); i++)
        {
            if(panels[i]->active() )
                panels[i]->on_update(ts.get_elapsed_millis());
        }

        Application::update(ts);
    }
    void Editor::train_splat_gaussian()
    {
        //auto current_splat_ent = get_current_splat_entt();
        while(true)
        {

        }
    }

    void Editor::update_gaussian(const TimeStep& ts)
    {
#ifdef DS_SPLAT_TRAIN

        auto gs_ent = Entity(current_train_entity, get_current_scene());
        if(!gs_ent.valid()) return;
        {
            auto& gs_train = gs_ent.get_component<GaussianTrainerScene>();
            if (gs_train.getCurrentTrainingStatus() == TrainingStatus::Preprocess_Done)
            {
                gs_train.setTrainingStatus(TrainingStatus::Training);
                auto& gs = gs_ent.get_component<GaussianComponent>();
                auto& gs_model = gs.ModelRef;
                auto [means, quats, scales, opacities, shs] = gs_train.getGaussianAttribute();
                gs_model->update_from_cpu(
                    means.data(),
                    shs.data(),
                    opacities.data(),
                    scales.data(),
                    quats.data(),
                    gs_train.getNumGaussians()
                );
                //set camera from training camera views
                auto transform = gs_ent.try_get_component<maths::Transform>();
                if (transform)
                {
                    auto viewR = gs_train.getCameraRotation(0);
                    //editor_cameraTransform.set_local_orientation(viewR);
                    editor_camera_transform.set_world_matrix(glm::mat4(1.0f));
                    focus_camera(transform->get_world_position(), 1.0f, 1.0f);
                    auto aabb = gs_model->get_local_bounding_box();
                    auto scale = (aabb.max() - aabb.min()) / 2.0f;
                    auto translate = aabb.center();
                    gs_train.focus_region_position = glm::vec3(0.0f);
                    gs_train.focus_region_scale = glm::vec3(1.0f);
                }
            }
            if (gs_train.getCurrentTrainingStatus() == TrainingStatus::Loading_Failed)
            {
                get_current_scene()->destroy_entity(gs_ent);
                if (gs_ent.get_handle() == current_train_entity) {
                    current_train_entity = entt::null;
                    gs_ent = Entity(current_train_entity, get_current_scene());
                }
            }
        }
        if (is_train_gaussian)
        {
            if (!gs_ent.valid() || !gs_ent.active())
                return;
            auto& gs_train = gs_ent.get_component<GaussianTrainerScene>();
            auto& gscom =  gs_ent.get_component<GaussianComponent>();
            auto& gs_model = gscom.ModelRef;
            const auto mouse_moved = Input::get().get_mouse_delta().x > 0 || Input::get().get_mouse_delta().y > 0 || Input::get().get_mouse_clicked(InputCode::MouseKey::ButtonLeft);
            const auto moved = editor_camera_controller.is_moving() | mouse_moved;
            gscom.skip_render = !moved;
            if (gs_train.getCurrentTrainingStatus() == TrainingStatus::Colmap_Sfm)
            {
                if (frame_number() % 100 == 0)
                {
                    set_gaussian_render_type(GaussianRenderType::Point);
                    const auto& points3d = gs_train.getPoints3D(0);
                    gs_model->update_from_pos_color(
                        (u8*)points3d.data(),
                        points3d.size()
                    );
                }
            }
            if (gs_train.isTrain() && gs_train.getCurrentTrainingStatus() >= TrainingStatus::Preprocess_Done)
            {
                if (gs_train.getCurrentIterations() == 0)
                {
                    if (!is_device_support_gstrain())
                    {
                        messageBox("warn", "current device compute capability doesn't support train");
                        gs_train.setTrainingStatus(TrainingStatus::Loading_Failed);
                    }
                    set_gaussian_render_type(GaussianRenderType::Splat);
                }
                if (is_device_support_gstrain())
                {
                    if(gs_train.getCurrentTrainingStatus() == TrainingStatus::Loading_Failed){
                        get_current_scene()->destroy_entity(gs_ent);
                    }
                    const bool is_training = gs_train.getCurrentIterations() < gs_train.getTrainConfig().numIters && !gs_train.isPruningSplat();
                    if (is_update_splat_rendering && is_training )
                    {
                        DS_LOG_INFO("Iteraions {}, loss : {}", gs_train.getCurrentIterations(), gs_train.getCurrentLoss());
                    }
                    if (is_update_splat_rendering && is_training)
                    {
                        gs_model->update_from_cpu(
                            gs_train.getGaussianPositionCpu().data(),
                            gs_train.getGaussianSHsCpu().data(),
                            gs_train.getGaussianOpcaitiesCpu().data(),
                            gs_train.getGaussianScalingsCpu().data(),
                            gs_train.getGaussianRotationsCpu().data(),
                            gs_train.getNumGaussians()
                        );
                        gscom.skip_render = false;
                        gs_model->antialiased() = gs_train.getTrainConfig().mipAntiliased;
                        if (gs_train.getNumGaussians() > 1000000) 
                        {
                            gs_train.getTrainConfig().packLevel |= GSPackLevel::PackTileID | GSPackLevel::PackF32ToU8;
                        }
                        is_update_splat_rendering = false;
                    }
                }
            }
            
        }
        else{
            if (gs_ent.valid() && gs_ent.active())
            {
                auto& gscom = gs_ent.get_component<GaussianComponent>();
                gscom.skip_render = false;
            } 
        }
#endif
    }

    void Editor::create_gaussian_dialog(const std::vector<std::string>& file_paths)
    {
#ifdef  DS_SPLAT_TRAIN
        std::string file_path;
        static int file_idx = 0;
        if (file_paths.size() > 1)
        {
            file_path = std::filesystem::path(file_paths[0]).parent_path().parent_path().string();
            auto image_path = file_path + "/images_" + std::to_string(file_idx);
            if (std::filesystem::exists(image_path))
            {
                image_path = file_path + "/images_" + std::to_string(file_idx + 1);
            }
            file_path = image_path;
        }
        else if (file_paths.size() < 10)
        {
            if(file_paths.size() > 0)
            {
                file_path = file_paths[0];
                if( is_image_file(file_path) || is_hdr_file(file_path))
                {
                    messageBox("warn", "trainning splat must input at least 10 view images!\n");
                    return;
                }
            }
        }
        ImGuiHelper::ScopedFont scopeFont(ImGui::GetIO().Fonts->Fonts.back());
        ImGui::Text("Create New GaussianSplat?\n\n");
        ImGui::Separator();

        static std::string newGSName = "3dgs";
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("name : ");
        ImGui::SameLine();
        ImGuiHelper::InputText(newGSName, "##GaussianNameChange");

        static std::string datasource_path;
        datasource_path = file_path;
        const auto is_video = is_video_file(file_path);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("data path ");
        ImGui::SameLine();
        ImGuiHelper::InputText(datasource_path, "##DataSourcePathChange");
        ImGui::SameLine();
        if( ImGuiHelper::Button("..")) //open file dialog
        {
            auto [browserPath,_] =  FileDialogs::openFile({"mp4","avi","m4v"});
            if(datasource_path.empty())
            {
                messageBox("warn", "datasource must be a valid path");
            }
            else
            {
                datasource_path = browserPath;
                splat_source_path = {datasource_path};
            }
        }

        auto gsModelPath = std::filesystem::path(file_path).parent_path();
        static std::string gs_output_path;
        gs_output_path = gsModelPath.string();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("output path : ");
        ImGui::SameLine();
        ImGuiHelper::InputText(gs_output_path, "##OutputPathChange");
        ImGui::SameLine();
        if(ImGuiHelper::Button("..",1)) //open file dialog
        {
            auto browserPath = diverse::FileDialogs::saveFile({ "ply", "splat", "compressply" });
            if (browserPath.empty())
            {
                messageBox("warn", "file must be a valid path");
            }
            else
                gs_output_path = browserPath;
        }
    
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        static bool useExternalCamPose = false;
        ImGuiHelper::Property("external camera poses", useExternalCamPose);
        ImGuiHelper::Tooltip("whether use external camera poses data!");
        ImGui::PopStyleVar();
        if(dropped_files.size() == 1)
        {
            auto drop_file_path = dropped_files[0];
            auto droped_ext = std::filesystem::path(drop_file_path).extension();
            int camerPosDataType = get_camera_pos_type_from_file(drop_file_path);
            if (camerPosDataType >= 0)
            {
                useExternalCamPose = true;
                trainConfig.cameraPosePath = drop_file_path;
                trainConfig.datasetType = camerPosDataType;
            }
            else if(droped_ext == ".ply" || droped_ext == ".bin")
            {
                trainConfig.pointCloudPath = drop_file_path;
            }
            dropped_files.clear();
        }
        if(useExternalCamPose)
        {
            ImGui::Columns(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("camera pose path : ");
            ImGui::SameLine();
            ImGuiHelper::InputText(trainConfig.cameraPosePath, "##PosePathChange");
            ImGui::SameLine();
            if(ImGuiHelper::Button("..",2)) //open file dialog
            {
                //const char* campose_str[] = {"Colmap","OpenSfm","RealityCapture","MetaShape"};
                auto [browserPath, camerPosDataType] = diverse::FileDialogs::openFile({ "json","bin", "csv","xml" },{"nerfstudio/opensfm/blender","colmap","realitycapture","metashape"});
                if (browserPath.empty())
                {
                    messageBox("warn", "must be a valid camera pose file");
                }
                else
                { 
                    trainConfig.cameraPosePath = browserPath;
                    camerPosDataType = get_camera_pos_type_from_file(browserPath);
                    trainConfig.datasetType = camerPosDataType;
                }
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("sparse pc path : ");
            ImGui::SameLine();
            ImGuiHelper::InputText(trainConfig.pointCloudPath, "##PCPathChange");
            ImGui::SameLine();
            if(ImGuiHelper::Button("..",3)) //open file dialog
            {
                auto [browserPath,_] = diverse::FileDialogs::openFile({ "ply","bin"});
                if (browserPath.empty())
                {
                    messageBox("warn", "file must be a valid point cloud file");
                }
                else
                    trainConfig.pointCloudPath = browserPath;
            }
        }
        diverse::ImGuiHelper::PushID();
        ImGui::Separator();
        if (!useExternalCamPose)
        {
            ImGui::Columns(1);
            if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_Framed))
            {
                ImGui::Columns(2);
                ImGui::TextUnformatted("Camera Model");
                ImGui::NextColumn();
                ImGui::PushItemWidth(-1);
                const char* camera_str[] = { "SIMPLE_PINHOLE" };
                if (ImGui::BeginCombo("camera model", camera_str[trainConfig.cameraModel], 0)) // The
                {
                    for (int n = 0; n < 1; n++) //now not support sparse grad
                    {
                        bool is_selected = (n == trainConfig.cameraModel);
                        if (ImGui::Selectable(camera_str[n]))
                        {
                            trainConfig.cameraModel = n;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ImGui::NextColumn();
                // static bool glomapper = trainConfig.mapperType == 2;
                ImGuiHelper::Property("Sfm Quality", trainConfig.quality, 0, 3, "set sparse reconstruct quality");
                // ImGuiHelper::Property("Use Glomapper", glomapper);
                // ImGuiHelper::Tooltip("whether use global bundle adjustment to estimate camera pose which will speed up the solution process but reduce accuracy.");
                ImGuiHelper::Property("Export Sfm", trainConfig.outputSparsePoints);
                ImGuiHelper::Tooltip("whether ouput colmap sparse point!");
                ImGuiHelper::Property("Share Single Camera",trainConfig.singleCamera);
                // trainConfig.mapperType = glomapper ? 2 : 0;
                ImGui::TreePop();
            }
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

        ImGui::Columns(2);
        ImGui::Separator();
      
        static bool packed = trainConfig.packLevel > 0;
        {
            ImGui::TextUnformatted("Splat Model");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            const char* model_str[] = { "3DGS", "2DGS"};
            if (ImGui::BeginCombo("splatmodel", model_str[trainConfig.modelType], 0)) // The second parameter is the label previewed before opening the combo.
            {
                for (int n = 0; n < 2; n++) //now not support sparse grad
                {
                    bool is_selected = (n == trainConfig.modelType);
                    if (ImGui::Selectable(model_str[n]))
                    {
                        trainConfig.modelType = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::NextColumn();

            ImGui::TextUnformatted("Densify Type");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            const char* densify_str[] = { "SplatADC", "SplatMCMC" };
            if (ImGui::BeginCombo("densify", densify_str[trainConfig.densifyStrategy], 0)) // The second parameter is the label previewed before opening the combo.
            {
                for (int n = 0; n < 2; n++) //now not support sparse grad
                {
                    bool is_selected = (n == trainConfig.densifyStrategy);
                    if (ImGui::Selectable(densify_str[n]))
                    {
                        trainConfig.densifyStrategy = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::NextColumn();
        }
        ImGui::TextUnformatted("Splat Format");
        ImGui::NextColumn();
        ImGui::PushItemWidth(-1);
        const char* format_str[] = { "ply", "splat", "compressed.ply", "spz"};
        static int cur_format = 0;
        if (ImGui::BeginCombo("", format_str[cur_format], 0)) // The second parameter is the label previewed before opening the combo.
        {
            for (int n = 0; n < 4; n++)
            {
                bool is_selected = (n == cur_format);
                if (ImGui::Selectable(format_str[n]))
                {
                   cur_format = n;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::NextColumn();
        //ImGui::Checkbox("Default Setup", &defaultSetup);
        ImGuiHelper::Property("MaxSplats",trainConfig.capMax);
        ImGuiHelper::Property("MaxImgNums", trainConfig.maxImageCount);
        ImGuiHelper::Property("MaxImgWidth", trainConfig.maxImageWidth);
        ImGuiHelper::Property("MaxImgHeight", trainConfig.maxImageHeight);
        if (is_video_file(file_path))
        {
            ImGuiHelper::Property("Fps", trainConfig.videoFps);
        }
  
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();

        ImGui::Separator();

        if (ImGui::TreeNodeEx("Advance", ImGuiTreeNodeFlags_Framed))
        {
            ImGui::Indent();
            ImGui::Columns(2);
            ImGuiHelper::Property("Create Sky Model",trainConfig.enableBg,"whether sky model!");
            ImGuiHelper::Property("Mask", trainConfig.useMask,"whether use mask!");
            ImGuiHelper::Property("ExportMesh", trainConfig.exportMesh,"whether create mesh model!");
            ImGuiHelper::Property("Anti-Alias", trainConfig.mipAntiliased, "whether enable antialias!");
            ImGui::Unindent();
            ImGui::TreePop();
        }
        ImGui::Columns(1);
        ImGui::Separator();

        const auto width = ImGui::GetWindowWidth();
        auto button_sizex = 120;
        auto button_posx = ImGui::GetCursorPosX() + (width / 2 - button_sizex) / 2;
        ImGui::SetCursorPosX(button_posx);
        if (ImGui::Button("OK", ImVec2(button_sizex, 0)) && !datasource_path.empty())
        {
            if (file_paths.size() > 1)
            {
                if (file_paths.size() > 1)
                {
                    if (!std::filesystem::exists(file_path))
                    {
                        std::filesystem::create_directories(file_path);
                        file_idx++;
                    }
                    for (const auto& p : file_paths)
                    {
                        const auto dstPath = file_path / std::filesystem::path(p).filename();
                        std::filesystem::copy(p, dstPath);
                    }
                }
            }
            if(!packed)
            {
                if(!is_video_file(file_path))
                { 
                    int file_count = 0;
                    for (const auto& entry : std::filesystem::directory_iterator(file_path)) {
                        if (entry.is_regular_file()) {
                            file_count++;
                        }
                    }
                    auto req_mem_size = file_count * sizeof(float) * 4 * (trainConfig.maxImageWidth * trainConfig.maxImageHeight);
                    auto total_vram_size = g_device->gpu_limits.vram_size;
                    if (req_mem_size >= total_vram_size * 0.33 && req_mem_size < 0.5 * total_vram_size) 
                        trainConfig.packLevel = GSPackLevel::PackF32ToU8;
                    else if(req_mem_size >= 0.5 * total_vram_size)
                        trainConfig.packLevel = GSPackLevel::PackF32ToU8 | GSPackLevel::PackTileID;
                    else
                        trainConfig.packLevel = 0;
                    if (req_mem_size >= total_vram_size * 0.66 ) 
                        trainConfig.img2gpuOnfly = true;
                    int times = (int)std::ceil(file_count / 500.0f);
                    trainConfig.pruneInterval = 700000 * times;
                    trainConfig.warmupLength = 500 * times;
                    trainConfig.numIters = 30000 * times;
                    trainConfig.resetAlphaEvery = trainConfig.refineEvery * 30;
                    trainConfig.refineStopIter = trainConfig.numIters / 2;
                    trainConfig.refineScale2dStopIter = trainConfig.refineStopIter / 3;
                    trainConfig.resolutionSchedule = 3000;
                }
            }
            trainConfig.cullSH = false;//cur_format >= 3;
            trainConfig.normalConsistencyLoss = trainConfig.exportMesh;
            Entity modelEntity = Application::get().get_current_scene()->create_entity();
            modelEntity.add_component<GaussianComponent>(trainConfig.capMax);
            auto& gaussian_train = modelEntity.add_component< GaussianTrainerScene>(trainConfig,-1);
            gaussian_train.setModelPath(gs_output_path + "/" + newGSName + "." + format_str[cur_format]);
            set_selected(modelEntity.get_handle());
            auto& transform = modelEntity.get_component<maths::Transform>();
            //rotate x axis -80.9 degree and y axis 40.7 degree and z axsi 140.7
            auto rotation = glm::vec3(180.0f, 0.0f, 0.0f);
            transform.set_local_orientation(glm::radians(rotation));
            transform.set_world_matrix(glm::mat4(1.0f));
            current_train_entity = modelEntity.get_handle();
            std::thread([&](){
                //try{
                    if(gaussian_train.loadTrainData(datasource_path)){
                        gaussian_train.trainSetup();
                    }
                    while(true)
                    {
                        if(gaussian_train.isTerminate()) break;
                        if(gaussian_train.isTrain() && is_train_gaussian)
                        {
                            if(!is_update_splat_rendering)
                            {
                                gaussian_train.trainStep();
                                auto curStep = gaussian_train.getCurrentIterations();
                                if(curStep % std::max<int>(10,splat_update_freq) == 0 
                                && curStep < gaussian_train.getTrainConfig().numIters)
                                    is_update_splat_rendering = true;
                                
                                if (curStep == gaussian_train.getTrainConfig().numIters - 1)
                                {
                                    gaussian_train.saveGaussianModel();
                                    if(gaussian_train.getTrainConfig().exportMesh)
                                    {
                                        DS_LOG_INFO("Extracting Gaussian Mesh.... ");
                                        try{
                                            auto mesh_path = std::filesystem::path(gaussian_train.getTrainConfig().modelPath).replace_extension("").string() + "_mesh.obj";
                                            gaussian_train.exportMesh(mesh_path);
                                            DS_LOG_INFO("Gaussian Mesh Exported to {}", mesh_path);
                                            //wait 1.5 second
                                            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                                            gs2mesh_load = true;
                                            load_model_path = mesh_path;
                                        }
                                        catch(const std::exception& e)
                                        {
                                            messageBox("error", e.what());
                                            gaussian_train.setTrainingStatus(TrainingStatus::Training);
                                            DS_LOG_ERROR(e.what());
                                        }
                                    }
                                }
                            }
                        }
                    }
                //}
       /*         catch(const std::exception& e)
                {
                    messageBox("error", e.what());
                    DS_LOG_ERROR(e.what());
                    gaussian_train.setTrainingStatus(TrainingStatus::Loading_Failed);
                }*/
            }).detach();

            ImGui::CloseCurrentPopup();
            is_train_gaussian = true;
            splat_source_path.clear();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::SetCursorPosX(button_posx + width / 2);
        if (ImGui::Button("Cancel", ImVec2(button_sizex, 0)))
        {
            ImGui::CloseCurrentPopup();
            splat_source_path.clear();
        }
#endif
    }

    void Editor::load_model_dialog(const std::string& filepath)
    {
        ImGui::Text("Load New Model?\n\n");
        ImGui::Separator();

        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();

        {
            ImGuiHelper::Property("import as a mesh", is_load_mesh);
            ImGuiHelper::Tooltip("whether import this model as a mesh");

            static bool import_normal = true;
            static bool import_optimized = false;
            if (is_load_mesh)
            {
                ImGuiHelper::Property("import mesh normal", import_normal);
                ImGuiHelper::Tooltip("whether import mesh model normal data");

                ImGuiHelper::Property("optimized mesh", import_optimized);
                ImGuiHelper::Tooltip("whether import optimized mesh data");
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();

        const auto width = ImGui::GetWindowWidth();
        auto button_sizex = 120;
        auto button_posx = ImGui::GetCursorPosX() + (width / 2 - button_sizex) / 2;
        ImGui::SetCursorPosX(button_posx);
        if (ImGui::Button("Yes", ImVec2(button_sizex, 0)) && !filepath.empty())
        {
            if (is_load_mesh)
            {
                Entity modelEntity = Application::get().get_scene_manager()->get_current_scene()->get_entity_manager()->create(stringutility::get_file_name(filepath));
                modelEntity.add_component<MeshModelComponent>(filepath);
                set_selected(modelEntity.get_handle());
                auto& trans = modelEntity.get_component<maths::Transform>();
                focus_camera(trans.get_local_position(), 2.0f, 2.0f);
                auto group = get_current_scene()->get_entity_manager()->get_entities_with_type<Environment>();
                for (auto ent : group)
                {
                    auto& enviroment = ent.get_component<Environment>();
                    if (enviroment.mode == Environment::Mode::Pure)
                    {
                        enviroment.mode = Environment::Mode::SunSky;
                        auto& sun_controller = ent.get_or_add_component<diverse::SunController>();
                        sun_controller.calculate_towards_sun_from_theta_phi(glm::radians(enviroment.theta), glm::radians(enviroment.phi));
                    }
                }
            }
            else if (is_load_point)
            {
                Entity modelEntity = Application::get().get_scene_manager()->get_current_scene()->get_entity_manager()->create(stringutility::get_file_name(filepath));
                modelEntity.add_component<PointCloudComponent>(filepath);
                auto& transform = modelEntity.get_component<maths::Transform>();
                auto rotation = glm::vec3(180.0f, 0.0f, 0.0f);
                transform.set_local_orientation(glm::radians(rotation));
                transform.set_world_matrix(glm::mat4(1.0f));
                focus_camera(transform.get_local_position(), 2.0f, 2.0f);
            }
            else
            {
                Entity modelEntity = Application::get().get_scene_manager()->get_current_scene()->get_entity_manager()->create(stringutility::get_file_name(filepath));
                modelEntity.add_component<GaussianComponent>(filepath);
                auto& transform = modelEntity.get_component<maths::Transform>();
                auto rotation = glm::vec3(180.0f, 0.0f, 0.0f);
                transform.set_local_orientation(glm::radians(rotation));
                transform.set_world_matrix(glm::mat4(1.0f));
                focus_camera(transform.get_local_position(), 2.0f, 2.0f);
                //set_selected(modelEntity.get_handle());
                //auto& ModelRef = modelEntity.get_component<GaussianComponent>().ModelRef;
                //if (ModelRef)
                //    ModelRef->splat_transforms.set_front_transform(transform.get_world_matrix());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::SetCursorPosX(button_posx + width / 2);
        if (ImGui::Button("No", ImVec2(button_sizex, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
    }
    void Editor::export_model_dialog()
    {
        ImGuiHelper::ScopedFont scopeFont(ImGui::GetIO().Fonts->Fonts.back());
        ImGui::Text("Export Model?\n\n");
        ImGui::Separator();

        diverse::ImGuiHelper::PushID();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        ImGui::Columns(2);
        ImGui::Separator();
        static bool apply_transform = false;
        ImGuiHelper::Property("export apply transform", apply_transform);
        ImGuiHelper::Tooltip("whether export transformed model ");
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::PopStyleVar();
        diverse::ImGuiHelper::PopID();

        static std::string filepath;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("export path : ");
        ImGui::SameLine();
        ImGuiHelper::InputText(filepath, "##ModelPathChange");
        ImGui::SameLine();
        if (ImGuiHelper::Button("..")) //open file dialog
        {
            if(!is_export_mesh)
                filepath = diverse::FileDialogs::saveFile({ "ply", "splat", "compressed.ply", "dvsplat","spz"});
            else
                filepath = diverse::FileDialogs::saveFile({ "obj", "ply"});
        }
        if (is_export_mesh)
        {
            ImGui::Columns(2);
            ImGui::Separator();
            ImGui::TextUnformatted("Mesh Quality");
            ImGui::NextColumn();
            ImGui::PushItemWidth(-1);
            const char* quality_str[] = { "Low","Medium","High",};
            static int cur_quality = 1;
            if (ImGui::BeginCombo("", quality_str[cur_quality], 0)) // The second parameter is the label previewed before opening the combo.
            {
                for (int n = 0; n < 3; n++)
                {
                    bool is_selected = (n == cur_quality);
                    if (ImGui::Selectable(quality_str[n]))
                    {
                        cur_quality = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            trainConfig.meshResolution = cur_quality == 0 ? 128 : (cur_quality == 1 ? 256 : 512);
            ImGui::PopItemWidth();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Separator();
        const auto width = ImGui::GetWindowWidth();
        auto button_sizex = 120;
        auto button_posx = ImGui::GetCursorPosX() + (width / 2 - button_sizex) / 2;
        ImGui::SetCursorPosX(button_posx);
        if (ImGui::Button("Yes", ImVec2(button_sizex, 0)) && !filepath.empty())
        {
            if(is_export_mesh)
            {
                System::JobSystem::Context context;
                System::JobSystem::execute(context, [&](JobDispatchArgs args) {
                    try{
                        auto& registry = get_current_scene()->get_registry();
                        for (auto gs_ent : selected_entities)
                        {
                            auto gs_com = registry.try_get<GaussianTrainerScene>(gs_ent);
                            if (gs_com)
                            {
                                gs_com->getTrainConfig().meshResolution = trainConfig.meshResolution;
                                gs_com->exportMesh(filepath);
                                DS_LOG_INFO("Gaussian Mesh Exported to {}", filepath);
                            }
                        }
                    }
                    catch(const std::exception& e)
                    {
                        messageBox("error", e.what());
                        DS_LOG_ERROR(e.what());
                    }
                });
                is_export_mesh = false;
            }
            else 
            {
                GaussianModel gs_model;
                if(exportType == 1)
                {
                    auto group = get_current_scene()->get_entity_manager()->get_entities_with_type<GaussianComponent>();
                    for (auto gs_ent : group)
                    {
                        if (!gs_ent.active())
                            continue;
                        auto& gs_com = gs_ent.get_component<GaussianComponent>();
                        GaussianComponent copy_gs(gs_com);
                        copy_gs.ModelRef = createSharedPtr<GaussianModel>();
                        copy_gs.ModelRef->merge(gs_com.ModelRef.get(),apply_transform);
                        copy_gs.apply_color_adjustment();
                        gs_model.merge(copy_gs.ModelRef.get());
                    }
                }
                else if (exportType == 2)
                {
                    auto& registry = get_current_scene()->get_registry();
                    for (auto gs_ent : selected_entities)
                    {
                        auto gs_com = registry.try_get<GaussianComponent>(gs_ent);
                        if (gs_com)
                        {
                            GaussianComponent copy_gs(*gs_com);
                            copy_gs.ModelRef = createSharedPtr<GaussianModel>();
                            copy_gs.ModelRef->merge(gs_com->ModelRef.get(), apply_transform);
                            copy_gs.apply_color_adjustment();
                            gs_model.merge(copy_gs.ModelRef.get());
                        }
                    }
                }
                if (gs_model.position().size() > 0 && !filepath.empty())
                {
                    gs_model.save_to_file(filepath);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        ImGui::SetCursorPosX(button_posx + width / 2);
        if (ImGui::Button("No", ImVec2(button_sizex, 0)))
        {
            is_export_mesh = false;
            ImGui::CloseCurrentPopup();
        }
    }
    void Editor::set_selected(entt::entity entity)
    {
        auto& splatEdit = GaussianEdit::get();
        auto& registry = Application::get().get_scene_manager()->get_current_scene()->get_registry();

        if (!registry.valid(entity))
            return;
        if (std::find(selected_entities.begin(), selected_entities.end(), entity) != selected_entities.end())
            return;
        auto transform = registry.try_get<maths::Transform>(entity);
        if(transform)
        {
            pivot->set_transform(*transform);
        }
        auto splat = registry.try_get<GaussianComponent>(entity);
        if (splat)
        {
            current_splat_entity = entity;
            splatEdit.set_edit_splat(splat);
            if(transform)
            {
                splatEdit.set_splat_transform(transform);
                if(splatEdit.has_select_gaussians())
                {
                    auto center = splat->ModelRef->get_selection_bounding_box(transform->get_world_matrix()).center();
                    pivot->get_transform().set_local_position(center);
                }
                else
                {
                    auto center = splat->ModelRef->get_world_bounding_box(transform->get_world_matrix()).center();
                    pivot->get_transform().set_local_position(center);
                }
                pivot->get_transform().set_world_matrix(glm::mat4(1.0f));
            }
            if(registry.try_get<GaussianTrainerScene>(entity))
            {
                current_train_entity = entity;
            }
        }

        selected_entities.push_back(entity);
    }

    void Editor::un_select(entt::entity entity)
    {
        auto it = std::find(selected_entities.begin(), selected_entities.end(), entity);

        if (it != selected_entities.end())
        {
            selected_entities.erase(it);
        }
    }

    void Editor::draw_menu_bar()
    {
        DS_PROFILE_FUNCTION();

        bool openSaveScenePopup = (scene_save_on_close && saveSceneShowAgain) || saveScenePopup;
        bool openNewScenePopup = newScenePopup;
        bool openProjectLoadPopup = false;//!m_ProjectLoaded;
        bool saveLayout = false;
        bool importModelPopup = false;
        auto resource_path = get_resource_path();
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGuiHelper::BeginMenu("Open Recent Project", resource_path + "svg_icon/open.png",!settings.recent_projects.empty()))
                {
                    for (auto& recentProject : settings.recent_projects)
                    {
                        if (ImGui::MenuItem(recentProject.c_str()))
                        {
                            Application::get().open_project(recentProject);

                            for (int i = 0; i < int(panels.size()); i++)
                            {
                                panels[i]->on_new_project();
                            }
                        }
                    }
                    ImGuiHelper::EndMenu();
                }

                ImGui::Separator();

                ImGui::Separator();
#ifdef DS_SPLAT_TRAIN
                if (ImGuiHelper::MenuItem("New GaussianSplat", resource_path + "svg_icon/new.png","CTRL+G"))
                {
                    is_open_newgaussian_popup = true;
                }
#endif                    
                if (ImGuiHelper::MenuItem("New Scene", resource_path + "svg_icon/new.png","CTRL+N"))
                {
                    openNewScenePopup = true;
                }

                if (ImGuiHelper::MenuItem("Save Scene", resource_path + "svg_icon/save.png", "CTRL+S"))
                {
                    openSaveScenePopup = true;
                }
                if (ImGuiHelper::MenuItem("Import Model", resource_path + "svg_icon/import.png"))
                {
                    importModelPopup = true;
                }
                ImGui::Separator();
                if (ImGuiHelper::BeginMenu("Export", resource_path + "svg_icon/export.png"))
                {
                    if (ImGuiHelper::MenuItem("Export All Splats", resource_path + "svg_icon/export.png"))
                    {
                        merge_splats();
                    }
                    if (ImGuiHelper::MenuItem("Export Select Splats", resource_path + "svg_icon/export.png"))
                    {
                        merge_select_splats();
                    }
                    #ifdef DS_SPLAT_TRAIN
                    if (ImGuiHelper::MenuItem("Export Camera", resource_path + "svg_icon/export.png"))
                    {
                        export_cameras();
                    }
                    if(ImGuiHelper::MenuItem("Export Sparse PointCloud", resource_path + "svg_icon/export.png"))
                    {
                        export_sparse_pointcloud();
                    }
                    if(ImGuiHelper::MenuItem("Export Mesh", resource_path + "svg_icon/export.png"))
                    {
                        export_mesh();
                    }
                    #endif
                    if (ImGuiHelper::MenuItem("Export WebViewer", resource_path + "svg_icon/export.png"))
                    {
                        export_webview();
                    }
                    ImGuiHelper::EndMenu();
                }

                ImGui::Separator();
                
                if (ImGui::BeginMenu(U8CStr2CStr(ICON_MDI_BORDER_STYLE "  Style###Style")))
                {
                    if (ImGui::MenuItem("Dark", "", settings.theme == ImGuiHelper::Dark))
                    {
                        settings.theme = ImGuiHelper::Dark;
                        ImGuiHelper::SetTheme(ImGuiHelper::Dark);
                        OS::instance()->setTitleBarColour(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
                    }
                    if (ImGui::MenuItem("Dracula", "", settings.theme == ImGuiHelper::Dracula))
                    {
                        settings.theme = ImGuiHelper::Dracula;
                        ImGuiHelper::SetTheme(ImGuiHelper::Dracula);
                        OS::instance()->setTitleBarColour(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
                    }
                    if (ImGui::MenuItem("Black", "", settings.theme == ImGuiHelper::Black))
                    {
                        settings.theme = ImGuiHelper::Black;
                        ImGuiHelper::SetTheme(ImGuiHelper::Black);
                        OS::instance()->setTitleBarColour(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_EXIT_TO_APP "  Exit###Exit")))
                {
                    Application::get().set_app_state(AppState::Closing);
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                //if(is_splat_edit)
                {
                    {
                        auto& splatEdit = GaussianEdit::get();
                        if (ImGuiHelper::BeginMenu(U8CStr2CStr(ICON_MDI_SELECTION "  SplatSelection###SplatSelection")))
                        {
                            if (ImGuiHelper::MenuItem("All", resource_path + "svg_icon/select-all.png","Ctrl + A"))
                                splatEdit.add_select_all_op();
                            if (ImGuiHelper::MenuItem("None", resource_path + "svg_icon/select-none.png","Shift + A"))
                                splatEdit.add_select_none_op();
                            if (ImGuiHelper::MenuItem("Invert", resource_path + "svg_icon/select-inverse.png","I"))
                                splatEdit.add_select_inverse_op();
                            if (ImGuiHelper::MenuItem("Lock", resource_path + "svg_icon/select-lock.png","H"))
                                splatEdit.add_lock_op();
                            if (ImGuiHelper::MenuItem("Unlock", resource_path + "svg_icon/select-unlock.png","U"))
                                splatEdit.add_unlock_op();
                            if (ImGuiHelper::MenuItem("Delete", resource_path + "svg_icon/select-delete.png","Delete"))
                                splatEdit.add_delete_op();
                            if (ImGuiHelper::MenuItem(U8CStr2CStr(ICON_MDI_RESTORE "  Reset###Reset"),nullptr,"R"))
                                splatEdit.add_reset_op();
                            if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_CUT "  Seperate###Seperate"), "CTRL+X", false))
                                splatEdit.add_seperate_selection_op();
                            if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_DUPLICATE "  Duplicate###Duplicate"), "CTRL+D", false))
                                splatEdit.add_duplicate_selection_op();
                            if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_COPY "  Duplicate+Sperate"), "CTRL+C", false))
                                splatEdit.add_duplicate_selection_2_instance_op();
                            ImGuiHelper::EndMenu();
                        }
                    }
                }
                bool enabled = !selected_entities.empty();

                if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_CUT "  Cut###Cut"), "CTRL+X", false, enabled))
                {
                    for (auto entity : selected_entities)
                        set_copied_entity(entity, true);
                }

                if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_COPY "  Copy###Copy"), "CTRL+C", false, enabled))
                {
                    for (auto entity : selected_entities)
                        set_copied_entity(entity, false);
                }

                enabled = !copied_entities.empty();

                if (ImGui::MenuItem(U8CStr2CStr(ICON_MDI_CONTENT_PASTE "  Paste###Paste"), "CTRL+V", false, enabled))
                {
                    for (auto entity : copied_entities)
                    {
                        auto scene = Application::get().get_current_scene();
                        scene->duplicate_entity({ entity, scene });
                    }
                }

#ifndef DS_PRODUCTION
                if (ImGui::MenuItem("EmbededTexture"))
                {
                    embed_editor_textures();
                }
                if (ImGui::MenuItem("EmbededShaders"))
                {
                    embeded_editor_shaders();
                }
#endif

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Scenes"))
            {
                auto scenes = Application::get().get_scene_manager()->get_scene_names();

                for (size_t i = 0; i < scenes.Size(); i++)
                {
                    auto name = scenes[i];
                    if (ImGui::MenuItem(name.c_str()))
                    {
                        Application::get().get_scene_manager()->switch_scene(name);
                    }
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                for (auto& panel : panels)
                {
                    if( helper_panels.get() == panel.get())
                        continue;
                    if(histogram_panel.get() == panel.get())
                        continue;
                    if (ImGui::MenuItem(panel->get_name().c_str(), "", &panel->active(), true))
                    {
                        panel->set_active(true);
                        if(panel == image_2d_panel && current_train_view_id < 0)
                            current_train_view_id = 0;
                    }
                }
                ImGui::Separator();

                bool layoutActive = true;
                if (ImGui::MenuItem("Save Layout"))
                    saveLayout = true;

                if (ImGui::MenuItem("Reset Layout"))
                {
                    ImGui::LoadIniSettingsFromDisk((layoutFolder + "dvui.ini").c_str());
                }
                //load layout
                if (ImGui::BeginMenu("Load Layout"))
				{
                    namespace fs = std::filesystem;
                    fs::path layoutPath =  layoutFolder;
                    if(fs::exists(layoutPath))
                    { 
                        for (const auto& entry : fs::directory_iterator(layoutPath))
                        {
                            if (entry.path().extension() == ".ini" && fs::is_regular_file(entry.status()))
                            {
                                //entry.path().filename().string();
                                if (ImGui::MenuItem(entry.path().filename().string().c_str()))
                                {
								    //load layout
                                    ImGui::LoadIniSettingsFromDisk(entry.path().string().c_str());
                                    DS_LOG_INFO("Load layout : {}", entry.path().filename().string());
							    }
                            }
                        }
                    }
                    ImGui::EndMenu();
				}
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Help"))
            {
                if(ImGui::MenuItem("About"))
                {
                     helper_panels->set_active(true);
                     helper_panels->set_tab(0);
                }
                ImGui::EndMenu();
            }

            if (project_loaded)
            {
                {
                    ImGuiHelper::ScopedFont boldFont(ImGui::GetIO().Fonts->Fonts[1]);
                    ImGuiHelper::ScopedColour border(ImGuiCol_Border, IM_COL32(40, 40, 40, 255));

                    ImGui::SameLine(ImGui::GetCursorPosX() + 40.0f);
                    ImGui::Separator();
                    ImGui::SameLine();
                    String8 label = PushStr8F(frame_arena, "%s ", get_current_scene()->get_scene_name().c_str());

                    ImGui::TextUnformatted((const char*)label.str);

                    String8 projectString = PushStr8F(frame_arena, "Current scene (%s.dsn)", get_current_scene()->get_scene_name().c_str());

                    ImGuiHelper::Tooltip((const char*)projectString.str);
                    ImGuiHelper::DrawBorder(ImGuiHelper::RectExpanded(ImGuiHelper::GetItemRect(), 24.0f, 68.0f), 1.0f, 3.0f, 0.0f, -60.0f);
                }
            }

            ImGui::SameLine((ImGui::GetWindowContentRegionMax().x * 0.5f) - (1.5f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)));

            //ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));
            bool selected;
#ifdef DS_SPLAT_TRAIN
            {
                selected = Application::get().get_editor_state() == EditorState::Play;
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiHelper::GetSelectedColour());
                auto gs = get_current_scene()->get_entity_manager()->get_entities_with_type<GaussianTrainerScene>();
                if( !gs.empty() )
                { 
                    auto& gs_train = gs[0].get_component<GaussianTrainerScene>();
                    if(!is_train_gaussian)
                    {
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_PLAY)))
                        {
                            is_train_gaussian = true;
                            gs_train.startTrain();
                        }
                        ImGuiHelper::Tooltip("TrainingPlay");
                    }
                    else
                    {
                        if (ImGui::Button(U8CStr2CStr(ICON_MDI_PAUSE)))
                        {
                            is_train_gaussian = false;
                            gs_train.pauseTrain();
                        }
                        ImGuiHelper::Tooltip("TrainingPause");
                    }
                }
                if (selected)
                    ImGui::PopStyleColor();
                if (!gs.empty())
                {
                    auto& gs_train = gs[0].get_component<GaussianTrainerScene>();
                    auto size = ImGui::CalcTextSize("%.i / %.i ") + ImGui::CalcTextSize("%.2f ms (%.i FPS)s");
                    float sizeOfGfxAPIDropDown = ImGui::GetFontSize() * 8;
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - size.x - ImGui::GetStyle().ItemSpacing.x * 10);
                    ImGui::TextColored(ImVec4(0,0.9,0,1), "%.i / %.i ", std::max(0,gs_train.curIteration), gs_train.getTrainConfig().numIters);
                }
            }
#endif
            ImGui::SameLine();

            static Engine::Stats stats = {};
            static double timer        = 1.1;
            timer += Engine::get_time_step().get_seconds();

            if(timer > 1.0)
            {
                timer = 0.0;
                stats = Engine::get().statistics();
            }

            auto size                  = ImGui::CalcTextSize("%.2f ms (%.i FPS)");
            float sizeOfGfxAPIDropDown = ImGui::GetFontSize() * 8;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - size.x - ImGui::GetStyle().ItemSpacing.x * 2);

            int fps = int(maths::Round(1000.0 / stats.FrameTime));
            ImGui::Text("%.2f ms (%.i FPS)", stats.FrameTime, fps);
            ImGui::EndMainMenuBar();
        }
        if(saveLayout)
            ImGui::OpenPopup("Save Layout");
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Save Layout", NULL,ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("LayoutName\n");
            ImGui::Separator();
            static std::string layoutname = "default";
            ImGuiHelper::InputText(layoutname, "##layoutname");
            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                
                if (!FileSystem::folder_exists(layoutFolder))
                    std::filesystem::create_directory(layoutFolder);
                auto inifileName = layoutFolder + std::string(layoutname) + ".ini";
                ImGui::SaveIniSettingsToDisk(inifileName.c_str());
                DS_LOG_INFO("Save layout to {}", inifileName);
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (openSaveScenePopup)
            ImGui::OpenPopup("Save Scene");

        bool  sceneSavePoped = false;
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Save Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Save Current Scene Changes?\n\n");
            ImGui::Separator();
            
            if(scene_save_on_close)
            { 
                static bool notshow = false;
                ImGui::Checkbox("Not Show Again", &notshow);
                saveSceneShowAgain = !notshow;
            }
            auto current_scene = get_current_scene();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Name : ");
            ImGui::SameLine();
            ImGuiHelper::InputText(current_scene->scene_name(), "##SceneName");
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_FOLDER)))
            {
                ImGui::CloseCurrentPopup();

                // Set filePath to working directory
                const auto& path = OS::instance()->getExecutablePath();
                auto [browserPath,_] = FileDialogs::openFile({});
                if (!browserPath.empty())
                {
                    projectLocation = browserPath;
                }
                else
                    locationPopupCanceled = true;
            }

            ImGui::SameLine();

            ImGui::TextUnformatted(projectLocation.c_str());

            ImGui::Separator();

            const auto btnSizeX = 80;
            if (ImGui::Button("OK", ImVec2(btnSizeX, 0)))
            {
                const auto path = std::filesystem::path(projectLocation + "/" + current_scene->scene_name());
                Application::get().open_new_project(projectLocation, current_scene->scene_name());
                get_current_scene()->serialise(project_settings.ProjectRoot + "assets/scenes/", false);
                //screenshot
                ImGui::CloseCurrentPopup();
                saveScenePopupCaneceld = false;
                scene_save_on_close = false;
            }
            ImGui::SetItemDefaultFocus();

            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(btnSizeX, 0)))
            {
                ImGui::CloseCurrentPopup();
                scene_save_on_close = false;
                saveScenePopupCaneceld = true;
                saveScenePopup = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(btnSizeX, 0)))
            {
                ImGui::CloseCurrentPopup();
                scene_save_on_close = false;
                saveScenePopupCaneceld = true;
                saveScenePopup = false;
                current_state = AppState::Running;
            }
            ImGui::EndPopup();
            sceneSavePoped = true;
        }
        if (!saveScenePopup && scene_save_on_close && !sceneSavePoped)
        {
            scene_save_on_close = false;
            if( !saveScenePopupCaneceld)
                Application::get().get_scene_manager()->get_current_scene()->serialise(project_settings.ProjectRoot + "assets/scenes/", false);
        }
        //else if (sceneSavePoped) //close
        //{
        //    m_SceneSaveOnClose = false;
        //}
        if (locationPopupOpened)
        {
            // Cancel clicked on project location popups
            if (locationPopupCanceled)
            {
                is_new_project_popup_open = false;
                locationPopupOpened = false;
                reopenNewProjectPopup = true;
            }
        }
        if (openNewScenePopup)
            ImGui::OpenPopup("New Scene");

        if ((reopenNewProjectPopup || openProjectLoadPopup) && !is_new_project_popup_open)
        {
            ImGui::OpenPopup("Open Project");
            reopenNewProjectPopup = false;
        }
        if (ImGui::BeginPopupModal("New Scene", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::Button("Save Current Scene Changes"))
            {
                Application::get().get_scene_manager()->get_current_scene()->serialise(project_settings.ProjectRoot + "assets/scenes/", false);
            }

            ImGui::Text("Create New Scene?\n\n");
            ImGui::Separator();

            static bool defaultSetup = true;

            static std::string newSceneName = "NewScene";
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Name : ");
            ImGui::SameLine();
            ImGuiHelper::InputText(newSceneName, "##SceneNameChange");

            // ImGui::Checkbox("Default Setup", &defaultSetup);
            if (ImGui::Button(U8CStr2CStr(ICON_MDI_FOLDER)))
            {
                ImGui::CloseCurrentPopup();

                is_new_project_popup_open = true;
                locationPopupOpened = true;

                // Set filePath to working directory
                const auto& path = OS::instance()->getExecutablePath();
                auto [browserPath,_] = FileDialogs::openFile({});
                if (!browserPath.empty())
                {
                    projectLocation = browserPath;
                }
                else
                    locationPopupCanceled = true;
            }

            ImGui::SameLine();

            ImGui::TextUnformatted(projectLocation.c_str());

            ImGui::Separator();
            const int buttonWidth = 120;
            if (ImGui::Button("OK", ImVec2(buttonWidth, 0)))
            {
                const auto path = std::filesystem::path(projectLocation + "/" + newSceneName);
                //if (!std::filesystem::exists(path))
                {
                    Application::get().open_new_project(projectLocation, newSceneName,true);
              
                    for (int i = 0; i < int(panels.size()); i++)
                    {
                        panels[i]->on_new_project();
                    }
                }
                auto scene = get_current_scene();
                std::string sceneName = newSceneName;
                int sameNameCount = 0;
                auto sceneNames = scene_manager->get_scene_names();

                while (FileSystem::file_exists(project_settings.ProjectRoot + "//assets/scenes/" + sceneName + ".dsn") || scene_manager->contains_scene(project_settings.ProjectRoot + "//assets/scenes/" + sceneName))
                {
                    sameNameCount++;
                    sceneName = fmt::format("{}{}",newSceneName,sameNameCount);
                }

                if (defaultSetup)
                {
                    auto environment = scene->get_entity_manager()->create("Environment");
                    environment.add_component<Environment>();
                    environment.get_component<Environment>().load();
                    
                    scene->serialise(project_settings.ProjectRoot + "assets/scenes/");
                }
                // Application::get().get_scene_manager()->enqueue_scene(scene);
                // Application::get().get_scene_manager()->switch_scene((int)(Application::get().get_scene_manager()->get_scenes().Size()) - 1);
                is_train_gaussian = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            newScenePopup = false;
        }
#ifdef DS_SPLAT_TRAIN
        if(is_open_newgaussian_popup)
            ImGui::OpenPopup("New GaussianSplat");
        if (ImGui::BeginPopupModal("New GaussianSplat", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            create_gaussian_dialog(splat_source_path);

            ImGui::EndPopup();
        }
#endif
        is_open_newgaussian_popup = false;
        if(importModelPopup)
        {
            auto [gs_path,_] = FileDialogs::openFile({"ply", "splat", "compressed.ply","dvsplat","spz","obj","gltf","glb"});
            if (is_gaussian_file(gs_path) || is_mesh_model_file(gs_path))
            {
                load_model_path = gs_path;
                is_load_model_popup = true;
                is_load_mesh = is_mesh_model_file(gs_path);
            }
            is_load_point = PointCloud::is_point_cloud_file(gs_path);
        }
        if(is_load_model_popup)
            ImGui::OpenPopup("Load Model");
        if(ImGui::BeginPopupModal("Load Model", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            load_model_dialog(load_model_path);
            ImGui::EndPopup();
        }
        is_load_model_popup = false;
        if(is_export_model_popup)
            ImGui::OpenPopup("Export Model");
        if (ImGui::BeginPopupModal("Export Model", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            export_model_dialog();
            ImGui::EndPopup();
        }
        is_export_model_popup = false;
    }

    static const float identityMatrix[16] = { 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f };

    void Editor::imguizmo(ImDrawList* drawList,
        const ImVec2& cameraPos,
        const ImVec2& windowPos,
        const ImVec2& canvasSize)
    {
        DS_PROFILE_FUNCTION();
        glm::mat4 view = glm::inverse(editor_camera_transform.get_world_matrix());
        glm::mat4 proj = current_camera->get_projection_matrix();

//#ifdef USE_IMGUIZMO_GRID
        if (settings.show_grid && !current_camera->is_orthographic())
            ImGuizmo::DrawGrid(glm::value_ptr(view),
                glm::value_ptr(proj), identityMatrix, 120.f);
        //ImGuizmo::DrawCubes(glm::value_ptr(view),
        //    glm::value_ptr(proj), identityMatrix, 1);
        // ImGuizmo::ViewManipulate(glm::value_ptr(view), 100.0f, ImVec2(canvasSize.x + windowPos.x - 50, windowPos.y ), ImVec2(48, 48), 0x10101010);
//#endif
        // float azim, elev;
        // if( ImGuizmo::ViewManipulateAxis(glm::value_ptr(view), 100.0f, ImVec2(canvasSize.x + windowPos.x - 50, windowPos.y + 60), azim, elev))
        // {
        //     editor_camera_transform.set_local_orientation(glm::vec3(elev,azim,0));
        // }
        ImOGuizmo::SetRect(canvasSize.x + windowPos.x - 96, windowPos.y + 32, 64.0f);
        if(ImOGuizmo::DrawGizmo(glm::value_ptr(view), glm::value_ptr(proj), 1.0f))
        {
            editor_camera_transform.set_local_orientation(glm::quat_cast(glm::inverse(view)));
        }
        if (!settings.show_gizmos || selected_entities.empty() || im_guizmo_operation == 4)
            return;

        auto& registry = get_current_scene()->get_registry();
        //box, sphere transform adjust
        {
            { 
                auto& splatEdit = GaussianEdit::get();
                if (splatEdit.get_edit_type() == diverse::GaussianEdit::EditType::Box || splatEdit.get_edit_type() == diverse::GaussianEdit::EditType::Sphere)
                {
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetOrthographic(current_camera->is_orthographic());

                    auto transform = &splatEdit.get_edit_transform();
                    if (transform != nullptr)
                    {
                        glm::mat4 model = transform->get_local_matrix();

                        float snapAmount[3] = { settings.snap_amount, settings.snap_amount, settings.snap_amount };
                        glm::mat4 deltaMatrix = glm::mat4(1.0f);

                        ImGuizmo::Manipulate(glm::value_ptr(view),
                            glm::value_ptr(proj),
                            static_cast<ImGuizmo::OPERATION>(im_guizmo_operation),
                            ImGuizmo::LOCAL,
                            glm::value_ptr(model),
                            glm::value_ptr(deltaMatrix),
                            settings.snap_quizmo ? snapAmount : nullptr);

                        if (ImGuizmo::IsUsing())
                        {
                            if (ImGuizmo::IsScaleType())
                            {
                                transform->set_local_scale(maths::get_scale(model));
                            }
                            else
                            {
                                transform->set_local_transform(model);
                            }
                        }
                    }
                    return;
                }
            }
        }
        if (selected_entities.size() == 1)
        {
            entt::entity m_SelectedEntity = entt::null;

            m_SelectedEntity = selected_entities.front();
            if (registry.valid(m_SelectedEntity))
            {
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetOrthographic(current_camera->is_orthographic());

                auto transform = registry.try_get<maths::Transform>(m_SelectedEntity);
                if (transform != nullptr)
                {
                    auto& pivot_transform = pivot->get_transform();
                    glm::mat4 model = pivot_transform.get_world_matrix();
                    // glm::mat4 model = transform->get_world_matrix();
                    float snapAmount[3] = { settings.snap_amount, settings.snap_amount, settings.snap_amount };
                    glm::mat4 deltaMatrix = glm::mat4(1.0f);

                    ImGuizmo::Manipulate(glm::value_ptr(view),
                        glm::value_ptr(proj),
                        static_cast<ImGuizmo::OPERATION>(im_guizmo_operation),
                        ImGuizmo::LOCAL,
                        glm::value_ptr(model),
                        glm::value_ptr(deltaMatrix),
                        settings.snap_quizmo ? snapAmount : nullptr);
                    static maths::Transform old_transform;
                    static bool imguizmo_manipulated = false;
                    if (ImGuizmo::IsUsing())
                    {
                        auto& splatEdit = GaussianEdit::get();
                        if (Input::get().get_mouse_clicked(InputCode::ButtonLeft))
                        {
                            old_transform = pivot_transform;
                            splatEdit.start_transform_op(pivot_transform);
                        }

                        Entity parent = Entity(m_SelectedEntity, scene_manager->get_current_scene()).get_parent();
                        if (parent && parent.has_component<maths::Transform>())
                        {
                            glm::mat4 parentTransform = parent.get_transform().get_world_matrix();
                            model = glm::inverse(parentTransform) * model;
                        }
                        if (ImGuizmo::IsScaleType())
                        {
                            glm::vec3 skew,scale,pos;
                            glm::vec4 perspective;
                            glm::quat rot;
                            glm::decompose(model, scale, rot, pos, skew, perspective);
                            pivot_transform.set_local_scale(scale);
                            transform->set_local_scale(scale);
                        }
                        else
                        {
                            if(!splatEdit.has_select_gaussians())
                                transform->set_local_transform(deltaMatrix * transform->get_local_matrix());
                            pivot_transform.set_local_transform(deltaMatrix * pivot_transform.get_local_matrix());
                        }
                        pivot_transform.set_world_matrix(glm::mat4(1.0f));
                        //if current select entity is gaussian splat
                        if(current_splat_entity == m_SelectedEntity)
                        {
                            splatEdit.update_transform_op(old_transform, pivot_transform);
                        }
                        imguizmo_manipulated = true;
                        invalidate_pt_state();

                    }
                    if (imguizmo_manipulated && !Input::get().get_mouse_held(InputCode::ButtonLeft))
                    {
                        auto& splatEdit = GaussianEdit::get();
                        splatEdit.end_transform_op(old_transform, pivot_transform,pivot.get());
                        imguizmo_manipulated = false;
                    }
                }
            }
        }
        else
        {
            glm::vec3 medianPointLocation = glm::vec3(0.0f);
            glm::vec3 medianPointScale = glm::vec3(0.0f);
            int validcount = 0;
            for (auto entityID : selected_entities)
            {
                if (!registry.valid(entityID))
                    continue;

                Entity entity = { entityID, scene_manager->get_current_scene() };

                if (!entity.has_component<maths::Transform>())
                    continue;

                medianPointLocation += entity.get_transform().get_world_position();
                medianPointScale += entity.get_transform().get_local_scale();
                validcount++;
            }
            medianPointLocation /= validcount; // selected_entities.size();
            medianPointScale /= validcount;    // selected_entities.size();

            glm::mat4 medianPointMatrix = glm::translate(glm::mat4(1.0f), medianPointLocation) * glm::scale(glm::mat4(1.0f), medianPointScale);

            ImGuizmo::SetDrawlist();
            ImGuizmo::SetOrthographic(current_camera->is_orthographic());

            float snapAmount[3] = { settings.snap_amount, settings.snap_amount, settings.snap_amount };
            glm::mat4 deltaMatrix = glm::mat4(1.0f);

            ImGuizmo::Manipulate(glm::value_ptr(view),
                glm::value_ptr(proj),
                static_cast<ImGuizmo::OPERATION>(im_guizmo_operation),
                ImGuizmo::LOCAL,
                glm::value_ptr(medianPointMatrix),
                glm::value_ptr(deltaMatrix),
                settings.snap_quizmo ? snapAmount : nullptr);

            if (ImGuizmo::IsUsing())
            {
                glm::vec3 deltaTranslation, deltaScale;
                glm::quat deltaRotation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(deltaMatrix, deltaScale, deltaRotation, deltaTranslation, skew, perspective);
                for (auto entityID : selected_entities)
                {
                    if (!registry.valid(entityID))
                        continue;
                    auto transform = registry.try_get<maths::Transform>(entityID);

                    if (!transform)
                        continue;
                    if (ImGuizmo::IsScaleType()) 
                    {
                        transform->set_local_scale(transform->get_local_scale() * deltaScale);
                    }
                    else if (ImGuizmo::IsRotateType())
                    {
                        transform->set_local_orientation(transform->get_local_orientation() + glm::eulerAngles(deltaRotation));
                    }
                    else
                    {
                        transform->set_local_transform(deltaMatrix * transform->get_local_matrix());
                    }
                }
            }
        }
    }

    void Editor::begin_dock_space(bool gameFullScreen)
    {
        DS_PROFILE_FUNCTION();
        static bool p_open = true;
        static bool opt_fullscreen_persistant = true;
        static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
        bool opt_fullscreen = opt_fullscreen_persistant;

        // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
        // because it would be confusing to have two docking targets within each others.
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            auto pos = viewport->Pos;
            auto size = viewport->Size;
            bool menuBar = true;
            if (menuBar)
            {
                const float infoBarSize = ImGui::GetFrameHeight();
                pos.y += infoBarSize;
                size.y -= infoBarSize;
            }

            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        // When using ImGuiDockNodeFlags_PassthruDockspace, DockSpace() will render our background and handle the
        // pass-thru hole, so we ask Begin() to not render a background.
        if (opt_flags & ImGuiDockNodeFlags_DockSpace)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MyDockspace", &p_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiID DockspaceID = ImGui::GetID("MyDockspace");

        static std::vector<SharedPtr<EditorPanel>> hiddenPanels;
        if (settings.full_screen_scene_view != gameFullScreen)
        {
            settings.full_screen_scene_view = gameFullScreen;

            if (settings.full_screen_scene_view)
            {
                for (auto panel : panels)
                {
                    if (panel->get_simple_name() != "Game" && panel->active())
                    {
                        panel->set_active(false);
                        hiddenPanels.push_back(panel);
                    }
                }
            }
            else
            {
                for (auto panel : hiddenPanels)
                {
                    panel->set_active(true);
                }

                hiddenPanels.clear();
            }
        }

        if (!ImGui::DockBuilderGetNode(DockspaceID))
        {
            ImGui::DockBuilderRemoveNode(DockspaceID); // Clear out existing layout
            ImGui::DockBuilderAddNode(DockspaceID);    // Add empty node
            ImGui::DockBuilderSetNodeSize(DockspaceID, ImGui::GetIO().DisplaySize * ImGui::GetIO().DisplayFramebufferScale);

            ImGuiID dock_main_id = DockspaceID;
            ImGuiID DockBottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.3f, nullptr, &dock_main_id);
            ImGuiID DockLeft = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
            ImGuiID DockRight = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);

            ImGuiID DockLeftChild = ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Down, 0.875f, nullptr, &DockLeft);
            ImGuiID DockRightChild = ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Down, 0.875f, nullptr, &DockRight);
            ImGuiID DockingLeftDownChild = ImGui::DockBuilderSplitNode(DockLeftChild, ImGuiDir_Down, 0.06f, nullptr, &DockLeftChild);
            ImGuiID DockingRightDownChild = ImGui::DockBuilderSplitNode(DockRightChild, ImGuiDir_Down, 0.06f, nullptr, &DockRightChild);

            ImGuiID DockBottomChild = ImGui::DockBuilderSplitNode(DockBottom, ImGuiDir_Down, 0.2f, nullptr, &DockBottom);
            ImGuiID DockingBottomLeftChild = ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Down, 0.4f, nullptr, &DockLeft);
            ImGuiID DockingBottomRightChild = ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Down, 0.4f, nullptr, &DockRight);

            ImGuiID DockMiddle = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.8f, nullptr, &dock_main_id);
            ImGuiID DockBottomMiddle = ImGui::DockBuilderSplitNode(DockMiddle, ImGuiDir_Down, 0.3f, nullptr, &DockMiddle);
            ImGuiID DockMiddleLeft = ImGui::DockBuilderSplitNode(DockMiddle, ImGuiDir_Left, 0.5f, nullptr, &DockMiddle);
            ImGuiID DockMiddleRight = ImGui::DockBuilderSplitNode(DockMiddle, ImGuiDir_Right, 0.5f, nullptr, &DockMiddle);

            ImGui::DockBuilderDockWindow("###Scene", DockMiddleLeft);
            ImGui::DockBuilderDockWindow("###Inspector", DockRight);
            ImGui::DockBuilderDockWindow("###Console", DockBottomMiddle);

            ImGui::DockBuilderDockWindow("###resources", DockingBottomLeftChild);
            ImGui::DockBuilderDockWindow("###KeyFrame", DockBottomMiddle);
            // ImGui::DockBuilderDockWindow("###Histogram", DockBottomMiddle);
            ImGui::DockBuilderDockWindow("###Hierarchy", DockRight);
            ImGui::DockBuilderDockWindow("###ScenePreview", DockLeft);
            ImGui::DockBuilderDockWindow("###SplatEdit", DockLeft);

            ImGui::DockBuilderFinish(DockspaceID);
        }

        // Dockspace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGui::DockSpace(DockspaceID, ImVec2(0.0f, 0.0f), opt_flags);
        }
    }

    void Editor::end_dock_space()
    {
        ImGui::End();
    }

    void Editor::handle_event(Event& e)
    {
        DS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowFileEvent>(BIND_EVENT_FN(Editor::handle_file_drop));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(Editor::handle_mouse_move));
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(Editor::handle_mouse_pressed));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(Editor::handle_mouse_released));
        
        // Block events here

        Application::handle_event(e);
    }

    void Editor::load_custom(cereal::JSONInputArchive& input, entt::snapshot_loader& snap_loader)
    {
#ifdef DS_SPLAT_TRAIN
        snap_loader.get<GaussianTrainerScene>(input)
#else
        snap_loader
#endif
                   .get<CamLenPreviewImageSet>(input)
                   .get<GaussianCrop>(input);
    }
    void Editor::load_custom(cereal::BinaryInputArchive& input, entt::snapshot_loader& snap_loader)
    {
#ifdef DS_SPLAT_TRAIN
        snap_loader.get<GaussianTrainerScene>(input)
#else
        snap_loader
#endif
                   .get<CamLenPreviewImageSet>(input)
                   .get<GaussianCrop>(input);
    }

    void Editor::save_custom(cereal::JSONOutputArchive& output, const entt::snapshot& snap)
    {
#ifdef DS_SPLAT_TRAIN
        snap.get<GaussianTrainerScene>(output)
#else
        snap
#endif
            .get<CamLenPreviewImageSet>(output)
            .get<GaussianCrop>(output);
    }
    void Editor::save_custom(cereal::BinaryOutputArchive& output, const entt::snapshot& snap)
    {
#ifdef DS_SPLAT_TRAIN
        snap.get<GaussianTrainerScene>(output)
#else
        snap
#endif
            .get<CamLenPreviewImageSet>(output)
            .get<GaussianCrop>(output);
    }

    void Editor::merge_splats()
    {
        exportType = 1;
        is_export_model_popup = true;
    }

    void Editor::merge_select_splats()
    {
        exportType = 2;
        is_export_model_popup = true;
    }

    void Editor::export_webview()
    {
        auto group = get_current_scene()->get_entity_manager()->get_entities_with_type<GaussianComponent>();
        GaussianModel gs_model;
        for (auto gs_ent : group)
        {
            if (!gs_ent.active())
                continue;
            auto& gs_com = gs_ent.get_component<GaussianComponent>();
            GaussianComponent copy_gs(gs_com);
            copy_gs.ModelRef = createSharedPtr<GaussianModel>();
            copy_gs.ModelRef->merge(gs_com.ModelRef.get());
            copy_gs.apply_color_adjustment();
            gs_model.merge(copy_gs.ModelRef.get());
        }
        if(gs_model.position().size() > 0)
        { 
            auto data = gs_model.get_compressed_data();
            auto ply_model = encode_base64(data);
            auto ply_model_loc = get_html_view_template.find("{{plyModel}}");
            auto html = get_html_view_template.replace(ply_model_loc,12,ply_model);
            auto color_loc = html.find("{{clearColor}}");
            html = html.replace(color_loc,14, "0.4,0.4,0.4");
            auto file_path = diverse::FileDialogs::saveFile({ "html"});
            if (file_path.empty())
                messageBox("warn", "file must be a valid path");
            else
            {
                std::ofstream ofs(file_path);
                if(ofs.is_open())
                {
                    ofs << html;
                    ofs.close();
                }
            }
        }
    }

#ifdef DS_SPLAT_TRAIN
    void Editor::export_cameras()
    {
        auto& registry = get_current_scene()->get_registry();
        if (selected_entities.size() <= 0) {
            messageBox("warn", "please select a splat model");
            return;
        }
        for (auto gs_ent : selected_entities)
        {
            auto gs_com = registry.try_get<GaussianTrainerScene>(gs_ent);
            if (gs_com)
            {
				auto file_path = diverse::FileDialogs::saveFile({ "json"});
				if (file_path.empty())
					messageBox("warn", "file must be a valid path");
                else
                {
                    gs_com->saveCameraDatas(file_path);
				}
			}
        }
    }

    void Editor::export_sparse_pointcloud()
    {
        auto& registry = get_current_scene()->get_registry();
        if (selected_entities.size() <= 0) {
            messageBox("warn", "please select a splat model");
            return;
        }
        for (auto gs_ent : selected_entities)
        {
            auto gs_com = registry.try_get<GaussianTrainerScene>(gs_ent);
            if (gs_com)
            {
                auto file_path = diverse::FileDialogs::saveFile({ "ply"});
				if (file_path.empty())
					messageBox("warn", "file must be a valid path");
                else
                {
                    gs_com->exportSparsePointCloud(file_path);
				}
            }
        }
    }

    void Editor::export_mesh()
    {
        auto& registry = get_current_scene()->get_registry();
        if (selected_entities.size() <= 0) {
            messageBox("warn", "please select a splat model");
            return;
        }
        for (auto gs_ent : selected_entities)
        {
            auto gs_com = registry.try_get<GaussianTrainerScene>(gs_ent);
            if (gs_com)
            {
                if (gs_com->getCurrentIterations() < gs_com->maxIteriaons()) {
                    messageBox("warn", "please make sure splat training is finished ");
                    return;
                }
                if(!gs_com->getTrainConfig().normalConsistencyLoss){
                    messageBox("warn", "please enable exportmesh option when train gaussian splat");
                    return;
                }
                is_export_model_popup = true;
                is_export_mesh = true;
            }
        }
    }

#endif
}
