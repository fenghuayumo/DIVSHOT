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
        : m_Frames(0)
        , m_Updates(0)
        , m_SceneViewWidth(800)
        , m_SceneViewHeight(600)
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

        m_FrameArena = ArenaAlloc(Kilobytes(64));
        m_Arena = ArenaAlloc(Kilobytes(64));

        set_max_image_dimensions(2048,2048);
        m_SceneManager = createUniquePtr<SceneManager>();
        create_default_project();

        //CommandLine* cmdline = CoreSystem::GetCmdLine();
        //if (cmdline->OptionBool(Str8Lit("help")))
        //{
        //    DS_LOG_INFO("Print this help.\n Option 1 : test");
        //}

        Engine::get();
        m_Timer = createUniquePtr<Timer>();
        //PythonManager::get().on_init();
        //PythonManager::get().on_new_project(m_ProjectSettings.m_ProjectRoot);

        WindowDesc windowDesc;
        windowDesc.Width = m_ProjectSettings.Width;
        windowDesc.Height = m_ProjectSettings.Height;
        windowDesc.RenderAPI = m_ProjectSettings.RenderAPI;
        windowDesc.Fullscreen = m_ProjectSettings.Fullscreen;
        windowDesc.Borderless = m_ProjectSettings.Borderless;
        windowDesc.ShowConsole = m_ProjectSettings.ShowConsole;
        windowDesc.Title = m_ProjectSettings.Title;
        windowDesc.VSync = m_ProjectSettings.VSync;
        if (m_ProjectSettings.DefaultIcon)
        {
    #ifdef DS_PLATFORM_WINDOWS
            windowDesc.IconPaths = { "../resource/app_icons/icon.png", "../resource/app_icons/icon32.png" };
    #elif defined(DS_PLATFORM_MACOS)
            windowDesc.IconPaths = { stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../Resources/icon.png",stringutility::get_file_location(OS::instance()->getExecutablePath()) +  "../Resources/icon32.png"};
    #endif
        }

        m_Window = UniquePtr<Window>(Window::create(windowDesc));
        if (!m_Window->has_initialised())
            quit();

        m_Window->set_event_callback(BIND_EVENT_FN(Application::handle_event));

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::StyleColorsDark();

        uint32_t screenWidth = m_Window->get_width();
        uint32_t screenHeight = m_Window->get_height();

        auto swapchain_extent = m_Window->get_frame_buffer_size();
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
            {  m_SceneManager->load_current_list(); });
        System::JobSystem::wait(context);


        m_Renderer = createUniquePtr<DeferedRenderer>(device,swap_chain.get());
        m_Renderer->init();
        
       m_ImGuiManager = createUniquePtr<ImGuiManager>(device,swap_chain.get(), m_Renderer->get_ui_renderer());
       m_ImGuiManager->init();
       DS_LOG_INFO("Initialised ImGui Manager");

        m_CurrentState = AppState::Running;
    }

    void Application::quit()
    {
        DS_PROFILE_FUNCTION();
        serialise();

        ArenaRelease(m_FrameArena);

        Engine::release();
        Input::release();

        m_Renderer.reset();
        m_ImGuiManager.reset();
        m_Window.reset();
        m_SceneManager.reset();
    }

    std::array<u32,2> Application::get_window_size() const
    {
        if (!m_Window)
            return {0, 0};
        return std::array<u32, 2>{m_Window->get_width(), m_Window->get_height()};
    }

    float Application::get_window_dpi() const
    {
        if (!m_Window)
            return 1.0f;

        return m_Window->get_dpi_scale();
    }


    Scene* Application::get_current_scene() const
    {
        DS_PROFILE_FUNCTION_LOW();
        return m_SceneManager->get_current_scene();
    }

    void Application::handle_new_scene(Scene * scene)
    {
        DS_PROFILE_FUNCTION();
        m_Renderer->handle_new_scene(scene);
    }

    bool Application::frame()
    {
        DS_PROFILE_FUNCTION();
        DS_PROFILE_FRAMEMARKER();
        ArenaClear(m_FrameArena);

        if (m_SceneManager->get_switching_scene())
        {
            DS_PROFILE_SCOPE("Application::SceneSwitch");
            //wait idle
            m_SceneManager->apply_scene_switch();
            return m_CurrentState != AppState::Closing;
        }

        double now = m_Timer->GetElapsedSD();
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
        m_Window->process_input();

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

            m_Updates++;
        }

        // Exit frame early if escape or close button clicked
        // Prevents a crash with vulkan/moltenvk
        if (m_CurrentState == AppState::Closing && !m_SceneSaveOnClose)
        {
            System::JobSystem::wait(context);
            return false;
        }

        if (!m_Minimized)
        {
            DS_PROFILE_SCOPE("Application::Render");
            
            m_ImGuiManager->render([&](){imgui_render();});
            DebugRenderer::Reset();
            debug_draw();
            render();
            m_Frames++;
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
            m_Window->update_cursor_imgui();
            m_Window->on_update();
        }

        {
            System::JobSystem::wait(context);
        }

        if (now - m_SecondTimer > 1.0f)
        {
            DS_PROFILE_SCOPE("Application::FrameRateCalc");
            m_SecondTimer += 1.0f;

            stats.FramesPerSecond = m_Frames;
            stats.UpdatesPerSecond = m_Updates;

            m_Frames = 0;
            m_Updates = 0;
        }
        if (!m_Minimized)
        {
            //present
        }
        return m_CurrentState != AppState::Closing || m_SceneSaveOnClose;
    }


    void Application::update_systems()
    {
        DS_PROFILE_FUNCTION();
    }

    void Application::render()
    {
        DS_PROFILE_FUNCTION();
        m_Renderer->render();
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
        if (!m_SceneManager->get_current_scene())
            return;


        if (Application::get().get_editor_state() != EditorState::Paused
            && Application::get().get_editor_state() != EditorState::Preview)
        {
            m_SceneManager->get_current_scene()->on_update(dt);
        }
    }

    void Application::handle_event(Event& e)
    {
        DS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::handle_window_close));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::handle_window_resize));

       if (m_ImGuiManager)
           m_ImGuiManager->handle_event(e);
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
            m_Minimized = true;
            return false;
        }
        m_Minimized = false;
        
        if (m_Renderer)
            m_Renderer->handle_window_resize(width, height);

        return false;
    }

    bool Application::handle_renderer_resize(u32 width, u32 height)
    {
        m_Renderer->handle_resize(width, height);
        return true;
    }

    bool Application::handle_window_close(WindowCloseEvent& e)
    {
        m_CurrentState = AppState::Closing;
        m_SceneSaveOnClose = true;
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
        return m_Renderer->get_main_render_image();
    }

    void Application::set_main_render_texture(std::shared_ptr<rhi::GpuTexture> texture)
    {
        m_Renderer->set_main_render_image(texture);
    }

    void Application::set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform)
    {
        m_Renderer->set_override_camera(camera, overrideCameraTransform);
    }

    int Application::frame_number() const
    {
        return m_Renderer->frame_idx;
    }

    void Application::refresh_shaders()
    {
        m_Renderer->refresh_shaders();
    }

    void Application::invalidate_pt_state()
    {
        m_Renderer->invalidate_pt_state();
    }

    void Application::open_project(const std::string& filePath)
    {
        DS_PROFILE_FUNCTION();
        m_ProjectSettings.m_ProjectName = stringutility::get_file_name(filePath);
        m_ProjectSettings.m_ProjectName = stringutility::remove_file_extension(m_ProjectSettings.m_ProjectName);

#ifndef DS_PLATFORM_IOS
        auto projectRoot = stringutility::get_file_location(filePath);
        m_ProjectSettings.m_ProjectRoot = projectRoot;
#endif

        m_SceneManager = createUniquePtr<SceneManager>();

        deserialise();

        m_SceneManager->load_current_list();
        m_SceneManager->apply_scene_switch();
    }

    void Application::open_new_project(const std::string& path, const std::string& name,bool create_scene)
    {
        DS_PROFILE_FUNCTION();
        m_ProjectSettings.m_ProjectRoot = path + "/" + name + "/";
        m_ProjectSettings.m_ProjectName = name;
        if (!FileSystem::folder_exists(path))
			std::filesystem::create_directory(path);

        std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot);

        mount_file_system_paths();
        // Set Default values
        m_ProjectSettings.RenderAPI = 1;
        m_ProjectSettings.Width = 1200;
        m_ProjectSettings.Height = 800;
        m_ProjectSettings.Borderless = false;
        m_ProjectSettings.VSync = true;
        m_ProjectSettings.Title = "SplatX";
        m_ProjectSettings.ShowConsole = false;
        m_ProjectSettings.Fullscreen = false;

#ifdef DS_PLATFORM_MACOS
        // This is assuming Application in bin/Release-macos-x86_64/diverseEditor.app
        DS_LOG_INFO(stringutility::get_file_location(OS::instance()->getExecutablePath()));
        m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";
#else
        m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif

        if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets");

        //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/scripts"))
        //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/scripts");

        if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/scenes"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/scenes");

        if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/textures"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/textures");

        if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/meshes"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/meshes");

        if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/previews"))
            std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/previews");
        //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/sounds"))
        //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/sounds");

        //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/prefabs"))
        //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/prefabs");

        //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/materials"))
        //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/materials");

        mount_file_system_paths();

        // Set Default values
        m_ProjectSettings.Title = "SplatX";
        m_ProjectSettings.Fullscreen = false;
        if (create_scene)
        {
            m_SceneManager = createUniquePtr<SceneManager>();
            m_SceneManager->enqueue_scene(new Scene(name));
            m_SceneManager->switch_scene(0);
            m_SceneManager->apply_scene_switch();
        }
   
        m_ProjectLoaded = true;

        serialise();

        //LuaManager::Get().OnNewProject(m_ProjectSettings.m_ProjectRoot);
    }

    void Application::mount_file_system_paths()
    {
        FileSystem::get().set_asset_root(PushStr8Copy(m_Arena, (m_ProjectSettings.m_ProjectRoot + std::string("assets")).c_str()));
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
            auto fullPath = m_ProjectSettings.m_ProjectRoot + m_ProjectSettings.m_ProjectName + std::string(".dvs");
            DS_LOG_INFO("Serialising Application {0}", fullPath);
            FileSystem::write_text_file(fullPath, storage.str());
        }
    }

    void Application::deserialise()
    {
        DS_PROFILE_FUNCTION();
        {
            auto filePath = m_ProjectSettings.m_ProjectRoot + m_ProjectSettings.m_ProjectName + std::string(".dvs");

            mount_file_system_paths();

            DS_LOG_INFO("Loading Project : {0}", filePath);

            if (!FileSystem::file_exists(filePath))
            {
                DS_LOG_INFO("No saved Project file found {0}", filePath);
                create_default_project();
                return;
            }

            if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets");

            //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/scripts"))
            //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/scripts");

            if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/scenes"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/scenes");

            if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/textures"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/textures");

            if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/meshes"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/meshes");

            if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/previews"))
                std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/previews");

            //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/sounds"))
            //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/sounds");

            //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/prefabs"))
            //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/prefabs");

            //if (!FileSystem::folder_exists(m_ProjectSettings.m_ProjectRoot + "assets/materials"))
            //    std::filesystem::create_directory(m_ProjectSettings.m_ProjectRoot + "assets/materials");

            m_ProjectLoaded = true;

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
                m_ProjectSettings.RenderAPI = 1;
                m_ProjectSettings.Width = 1200;
                m_ProjectSettings.Height = 800;
                m_ProjectSettings.Borderless = false;
                m_ProjectSettings.VSync = true;
                m_ProjectSettings.Title = "SplatX";
                m_ProjectSettings.ShowConsole = false;
                m_ProjectSettings.Fullscreen = false;

#ifdef DS_PLATFORM_MACOS
                m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";
#else
                m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif

                m_SceneManager->enqueue_scene(new Scene("Empty Scene"));
                m_SceneManager->switch_scene(0);

                DS_LOG_ERROR("Failed to load project - {0}", filePath);
            }
        }
    }

    void Application::create_default_project()
    {
        m_SceneManager = createUniquePtr<SceneManager>();

        // Set Default values
        m_ProjectSettings.RenderAPI = 1;
        m_ProjectSettings.Width = 1200;
        m_ProjectSettings.Height = 800;
        m_ProjectSettings.Borderless = false;
        m_ProjectSettings.VSync = true;
        m_ProjectSettings.Title = "SplatX";
        m_ProjectSettings.ShowConsole = false;
        m_ProjectSettings.Fullscreen = false;

        m_ProjectLoaded = false;

#ifdef DS_PLATFORM_MACOS
        // This is assuming Application in bin/Release-macos-x86_64/diverse_editor.app
        DS_LOG_INFO(stringutility::get_file_location(OS::instance()->getExecutablePath()));
        m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../../../../diverse/assets/";

        if (!FileSystem::folder_exists(m_ProjectSettings.m_EngineAssetPath))
        {
            m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
        }
#else
        m_ProjectSettings.m_EngineAssetPath = stringutility::get_file_location(OS::instance()->getExecutablePath()) + "../../diverse/assets/";
#endif
        m_SceneManager->enqueue_scene(new Scene("Empty Scene"));
        m_SceneManager->switch_scene(0);
    }

    void Application::add_default_scene()
    {
        if (m_SceneManager->get_scenes().Size() == 0)
        {
            m_SceneManager->enqueue_scene(new Scene("Empty Scene"));
            m_SceneManager->switch_scene(0);
        }
    }
}
