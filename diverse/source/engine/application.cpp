#include <map>
#include "renderer/defered_renderer.h"
#include "renderer/debug_renderer.h"
#include "application.h"
#include "engine.h"
#include "utility/timer.h"
#include "engine/input.h"
#include "engine/window.h"
#include "engine/os.h"
#include "engine/file_system.h"
#include "engine/core_system.h"
#include "core/profiler.h"
#include "core/job_system.h"
#include "events/application_event.h"
#include "imgui/imgui_manager.h"
#include "utility/load_image.h"
#include "utility/string_utils.h"
#include "scene/scene_manager.h"
#include "scene/scene.h"
//#include "scripting/python/python_manager.h"
#if __has_include(<filesystem>)
#include <filesystem>
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
#endif

#include <cereal/archives/json.hpp>
#include <imgui/imgui.h>
#include <imgui/Plugins/implot/implot.h>


namespace diverse
{
    Application* Application::s_Instance = nullptr;

    Application::Application()
        : frame_cnts(0)
        , update_cnts(0)
        , scene_view_width(800)
        , scene_view_height(600)
    {
        DS_PROFILE_FUNCTION();
        DS_ASSERT(!s_Instance, "Application already exists!");

        s_Instance = this;
    }

    Application::~Application()
    {
        DS_PROFILE_FUNCTION();
        ImGui::DestroyContext();
        ImPlot::DestroyContext();
        destroy_device();
    }

    void Application::init()
    {
        DS_PROFILE_FUNCTION();

        frame_arena = ArenaAlloc(Kilobytes(64));
        arena = ArenaAlloc(Kilobytes(64));

        set_max_image_dimensions(2048,2048);
        scene_manager = createUniquePtr<SceneManager>();
        create_default_project();

        //CommandLine* cmdline = CoreSystem::GetCmdLine();
        //if (cmdline->OptionBool(Str8Lit("help")))
        //{
        //    DS_LOG_INFO("Print this help.\n Option 1 : test");
        //}

        Engine::get();
        timer = createUniquePtr<Timer>();
        //PythonManager::get().on_init();
        //PythonManager::get().on_new_project(project_settings.m_ProjectRoot);

        WindowDesc windowDesc;
        windowDesc.Width = project_settings.Width;
        windowDesc.Height = project_settings.Height;
        windowDesc.RenderAPI = project_settings.RenderAPI;
        windowDesc.Fullscreen = project_settings.Fullscreen;
        windowDesc.Borderless = project_settings.Borderless;
        windowDesc.ShowConsole = project_settings.ShowConsole;
        windowDesc.Title = project_settings.Title;
        windowDesc.VSync = project_settings.VSync;
        if (project_settings.DefaultIcon)
        {
        #ifdef DS_PLATFORM_WINDOWS
            windowDesc.IconPaths = { "../resource/app_icons/icon.png", "../resource/app_icons/icon32.png" };
        #elif defined(DS_PLATFORM_MACOS)
            windowDesc.IconPaths = { stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../Resources/icon.png",stringutility::get_file_location(OS::instance()->getExecutablePath()) +  "../Resources/icon32.png"};
        #endif
        }

        window = UniquePtr<Window>(Window::create(windowDesc));
        if (!window->has_initialised())
            quit();

        window->set_event_callback(BIND_EVENT_FN(Application::handle_event));

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();

        uint32_t screenWidth = window->get_width();
        uint32_t screenHeight = window->get_height();

        auto swapchain_extent = window->get_frame_buffer_size();
        //#ifdef DS_PLATFORM_MACOS
        //       auto device = rhi::create_device(-1, RenderAPI::METAL);
        //#else
        auto device = rhi::create_device(-1, RenderAPI::VULKAN);
        //#endif
        auto swap_chain = device->create_swapchain(rhi::SwapchainDesc{ swapchain_extent, false }, Application::get().get_window());

        System::JobSystem::Context context;

        System::JobSystem::execute(context, [](JobDispatchArgs args)
            { diverse::Input::get(); });

        System::JobSystem::execute(context, [this](JobDispatchArgs args)
            {  scene_manager->load_current_list(); });
        System::JobSystem::wait(context);


        main_renderer = createUniquePtr<DeferedRenderer>(device,swap_chain.get());
        main_renderer->init();

        imgui_manager = createUniquePtr<ImGuiManager>(device,swap_chain.get(), main_renderer->get_ui_renderer());
        imgui_manager->init();
        DS_LOG_INFO("Initialised ImGui Manager");

        current_state = AppState::Running;
    }

    void Application::quit()
    {
        DS_PROFILE_FUNCTION();
        serialise();

        ArenaRelease(frame_arena);

        Engine::release();
        Input::release();

        main_renderer.reset();
        imgui_manager.reset();
        window.reset();
        scene_manager.reset();
    }

    std::array<u32,2> Application::get_window_size() const
    {
        if (!window)
            return {0, 0};
        return std::array<u32, 2>{window->get_width(), window->get_height()};
    }

    float Application::get_window_dpi() const
    {
        if (!window)
            return 1.0f;

        return window->get_dpi_scale();
    }


    Scene* Application::get_current_scene() const
    {
        DS_PROFILE_FUNCTION_LOW();
        return scene_manager->get_current_scene();
    }

    void Application::handle_new_scene(Scene * scene)
    {
        DS_PROFILE_FUNCTION();
        main_renderer->handle_new_scene(scene);
    }

    bool Application::frame()
    {
        DS_PROFILE_FUNCTION();
        DS_PROFILE_FRAMEMARKER();
        ArenaClear(frame_arena);

        if (scene_manager->get_switching_scene())
        {
            DS_PROFILE_SCOPE("Application::SceneSwitch");
            //wait idle
            scene_manager->apply_scene_switch();
            return current_state != AppState::Closing;
        }

        double now = timer->GetElapsedSD();
        auto& stats = Engine::get().statistics();
        auto& ts = Engine::get_time_step();

        if (ts.get_seconds() > 5)
        {
            // DS_LOG_WARN("Large frame time {0}", ts.get_seconds());
#ifdef DS_DISABLE_LARGE_FRAME_TIME
            // Added to stop application locking computer
            // Exit if frametime exceeds 5 seconds
            return false;
#endif
        }

        {
           DS_PROFILE_SCOPE("Application::TimeStepUpdates");
           ts.update();

           ImGuiIO& io = ImGui::GetIO();
           io.DeltaTime = (float)ts.get_seconds();
           stats.FrameTime = ts.get_millis();
        }

        Input::get().reset_pressed();
        window->process_input();

        {
            DS_PROFILE_SCOPE("ImGui::NewFrame");
            ImGui::NewFrame();
        }

        System::JobSystem::Context context;
        {
            DS_PROFILE_SCOPE("Application::Update");
            update(ts);

            System::JobSystem::execute(context, [](JobDispatchArgs args)
                { Application::update_systems(); });

            update_cnts++;
        }

        // Exit frame early if escape or close button clicked
        // Prevents a crash with vulkan/moltenvk
        if (current_state == AppState::Closing && !scene_save_on_close)
        {
            System::JobSystem::wait(context);
            return false;
        }

        if (!is_minimized)
        {
            DS_PROFILE_SCOPE("Application::Render");
            
            imgui_manager->render([&](){imgui_render();});
            DebugRenderer::Reset();
            debug_draw();
            render();
            frame_cnts++;
        }
        else
        {
            ImGui::Render();
        }

        {
            DS_PROFILE_SCOPE("Application::UpdateGraphicsStats");
        }

        {
            DS_PROFILE_SCOPE("Application::WindowUpdate");
            window->update_cursor_imgui();
            window->on_update();
        }

        {
            System::JobSystem::wait(context);
        }

        if (now - second_timer > 1.0f)
        {
            DS_PROFILE_SCOPE("Application::FrameRateCalc");
            second_timer += 1.0f;

            stats.FramesPerSecond = frame_cnts;
            stats.UpdatesPerSecond = update_cnts;

            frame_cnts = 0;
            update_cnts = 0;
        }
        if (!is_minimized)
        {
            //present
        }
        return current_state != AppState::Closing || scene_save_on_close;
    }


    void Application::update_systems()
    {
        DS_PROFILE_FUNCTION();
    }

    void Application::render()
    {
        DS_PROFILE_FUNCTION();
        main_renderer->render();
    }

    void Application::imgui_render()
    {
        DS_PROFILE_FUNCTION();

    }

    void Application::debug_draw()
    {

    }

    void Application::update(const TimeStep& dt)
    {
        DS_PROFILE_FUNCTION();
        if (!scene_manager->get_current_scene())
            return;


        if (Application::get().get_editor_state() != EditorState::Paused
            && Application::get().get_editor_state() != EditorState::Preview)
        {
            scene_manager->get_current_scene()->on_update(dt);
        }
    }

    void Application::handle_event(Event& e)
    {
        DS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::handle_window_close));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::handle_window_resize));

        if (imgui_manager)
            imgui_manager->handle_event(e);
        if (e.Handled())
            return;

        if (e.Handled())
            return;

        Input::get().on_event(e);
    }

    bool Application::handle_window_resize(WindowResizeEvent& e)
    {
        DS_PROFILE_FUNCTION();

        int width = e.GetWidth(), height = e.GetHeight();

        if (width == 0 || height == 0)
        {
            is_minimized = true;
            return false;
        }
        is_minimized = false;
        
        if (main_renderer)
            main_renderer->handle_window_resize(width, height);

        return false;
    }

    bool Application::handle_renderer_resize(u32 width, u32 height)
    {
        main_renderer->handle_resize(width, height);
        return true;
    }

    bool Application::handle_window_close(WindowCloseEvent& e)
    {
        current_state = AppState::Closing;
        scene_save_on_close = true;
        return true;
    }

    void Application::run()
    {
        while (frame())
        {
        }

        quit();
    }

    std::shared_ptr<rhi::GpuTexture> Application::get_main_render_texture()
    {
        return main_renderer->get_main_render_image();
    }

    void Application::set_main_render_texture(std::shared_ptr<rhi::GpuTexture> texture)
    {
        main_renderer->set_main_render_image(texture);
    }

    void Application::set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform)
    {
        main_renderer->set_override_camera(camera, overrideCameraTransform);
    }

    int Application::frame_number() const
    {
        return main_renderer->frame_idx;
    }

    void Application::refresh_shaders()
    {
        main_renderer->refresh_shaders();
    }

    void Application::invalidate_pt_state()
    {
        main_renderer->invalidate_pt_state();
    }

    void Application::open_project(const std::string& filePath)
    {
        DS_PROFILE_FUNCTION();
        project_settings.ProjectName = stringutility::get_file_name(filePath);
        project_settings.ProjectName = stringutility::remove_file_extension(project_settings.ProjectName);

#ifndef DS_PLATFORM_IOS
        auto projectRoot = stringutility::get_file_location(filePath);
        project_settings.ProjectRoot = projectRoot;
#endif

        scene_manager = createUniquePtr<SceneManager>();

        deserialise();

        scene_manager->load_current_list();
        scene_manager->apply_scene_switch();
    }

    void Application::open_new_project(const std::string& path, const std::string& name,bool create_scene)
    {
        DS_PROFILE_FUNCTION();
        project_settings.ProjectRoot = path + "/" + name + "/";
        project_settings.ProjectName = name;
        if (!FileSystem::folder_exists(path))
			std::filesystem::create_directory(path);

        std::filesystem::create_directory(project_settings.ProjectRoot);

        mount_file_system_paths();
        // Set Default values
        project_settings.RenderAPI = 1;
        project_settings.Width = 1200;
        project_settings.Height = 800;
        project_settings.Borderless = false;
        project_settings.VSync = false;
        project_settings.Title = "divshot";
        project_settings.ShowConsole = false;
        project_settings.Fullscreen = false;

#ifdef DS_PLATFORM_MACOS
        // This is assuming Application in bin/Release-macos-x86_64/diverseEditor.app
        DS_LOG_INFO(stringutility::get_file_location(OS::instance()->getExecutablePath()));
        project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";
#else
        project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif

        if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets"))
            std::filesystem::create_directory(project_settings.ProjectRoot + "assets");
            
        if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/scenes"))
            std::filesystem::create_directory(project_settings.ProjectRoot + "assets/scenes");

        if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/textures"))
            std::filesystem::create_directory(project_settings.ProjectRoot + "assets/textures");

        if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/meshes"))
            std::filesystem::create_directory(project_settings.ProjectRoot + "assets/meshes");

        if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/previews"))
            std::filesystem::create_directory(project_settings.ProjectRoot + "assets/previews");

        mount_file_system_paths();

        // Set Default values
        project_settings.Title = "divshot";
        project_settings.Fullscreen = false;
        if (create_scene)
        {
            scene_manager = createUniquePtr<SceneManager>();
            scene_manager->enqueue_scene(new Scene(name));
            scene_manager->switch_scene(0);
            scene_manager->apply_scene_switch();
        }
   
        project_loaded = true;

        serialise();

    }

    void Application::mount_file_system_paths()
    {
        FileSystem::get().set_asset_root(PushStr8Copy(arena, (project_settings.ProjectRoot + std::string("assets")).c_str()));
    }

    void Application::serialise()
    {
        DS_PROFILE_FUNCTION();
        {
            std::stringstream storage;
            {
                // output finishes flushing its contents when it goes out of scope
                cereal::JSONOutputArchive output{ storage };
                output(*this);
            }
            auto fullPath = project_settings.ProjectRoot + project_settings.ProjectName + std::string(".dvs");
            DS_LOG_INFO("Serialising Application {0}", fullPath);
            FileSystem::write_text_file(fullPath, storage.str());
        }
    }

    void Application::deserialise()
    {
        DS_PROFILE_FUNCTION();
        {
            auto filePath = project_settings.ProjectRoot + project_settings.ProjectName + std::string(".dvs");

            mount_file_system_paths();

            DS_LOG_INFO("Loading Project : {0}", filePath);

            if (!FileSystem::file_exists(filePath))
            {
                DS_LOG_INFO("No saved Project file found {0}", filePath);
                create_default_project();
                return;
            }

            if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets"))
                std::filesystem::create_directory(project_settings.ProjectRoot + "assets");

            //if (!FileSystem::folder_exists(project_settings.m_ProjectRoot + "assets/scripts"))
            //    std::filesystem::create_directory(project_settings.m_ProjectRoot + "assets/scripts");

            if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/scenes"))
                std::filesystem::create_directory(project_settings.ProjectRoot + "assets/scenes");

            if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/textures"))
                std::filesystem::create_directory(project_settings.ProjectRoot + "assets/textures");

            if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/meshes"))
                std::filesystem::create_directory(project_settings.ProjectRoot + "assets/meshes");

            if (!FileSystem::folder_exists(project_settings.ProjectRoot + "assets/previews"))
                std::filesystem::create_directory(project_settings.ProjectRoot + "assets/previews");

            project_loaded = true;

            std::string data = FileSystem::read_text_file(filePath);
            std::istringstream istr;
            istr.str(data);
            try
            {
                cereal::JSONInputArchive input(istr);
                input(*this);
            }
            catch (...)
            {
                // Set Default values
                project_settings.RenderAPI = 1;
                project_settings.Width = 1200;
                project_settings.Height = 800;
                project_settings.Borderless = false;
                project_settings.VSync = false;
                project_settings.Title = "divshot";
                project_settings.ShowConsole = false;
                project_settings.Fullscreen = false;

#ifdef DS_PLATFORM_MACOS
                project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";
#else
                project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif

                scene_manager->enqueue_scene(new Scene("Empty Scene"));
                scene_manager->switch_scene(0);

                DS_LOG_ERROR("Failed to load project - {0}", filePath);
            }
        }
    }

    void Application::create_default_project()
    {
        scene_manager = createUniquePtr<SceneManager>();

        // Set Default values
        project_settings.RenderAPI = 1;
        project_settings.Width = 1200;
        project_settings.Height = 800;
        project_settings.Borderless = false;
        project_settings.VSync = false;
        project_settings.Title = "divshot";
        project_settings.ShowConsole = false;
        project_settings.Fullscreen = false;

        project_loaded = false;

#ifdef DS_PLATFORM_MACOS
        // This is assuming Application in bin/Release-macos-x86_64/diverse_editor.app
        DS_LOG_INFO(stringutility::get_file_location(OS::instance()->getExecutablePath()));
        project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";

        if (!FileSystem::folder_exists(project_settings.EngineAssetPath))
        {
            project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
        }
#else
        project_settings.EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif
        scene_manager->enqueue_scene(new Scene("Empty Scene"));
        scene_manager->switch_scene(0);
    }

    void Application::add_default_scene()
    {
        if (scene_manager->get_scenes().Size() == 0)
        {
            scene_manager->enqueue_scene(new Scene("Empty Scene"));
            scene_manager->switch_scene(0);
        }
    }
}
