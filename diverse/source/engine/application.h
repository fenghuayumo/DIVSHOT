#pragma once
#include "core/reference.h"
#include "utility/time_step.h"
#include <thread>
#include <cereal/types/vector.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <queue>
#include <glm/vec2.hpp>
#include "maths/transform.h"
#include "scene/scene_manager.h"
#include "engine/file_system.h"
#include <entt/entity/registry.hpp>
namespace diverse
{
    
    namespace rhi 
    {
        struct GpuTexture;
    }
    class Timer;
    class Window;
    struct WindowDesc;
    class Event;
    class WindowCloseEvent;
    class WindowResizeEvent;
    class ImGuiManager;
    class SceneManager;
    class Scene;
    class Camera;

    enum class AppState
    {
        Running,
        Loading,
        Closing
    };

    enum class EditorState
    {
        Paused,
        Play,
        Next,
        Preview
    };

    enum class AppType
    {
        Game,
        Editor
    };

    class DeferedRenderer;

  //  enum EngineShowFlag : u32
  //  {
  //      ShowGrid = 1,
		//ShowGizmo = 2,
		//ShowSelected = 4,
		//ShowCameraFrustum = 8,
		//ShowMeshBoundingBoxes = 16,
		//ShowSpriteBoxes = 32,
		//ShowEntityNames = 64,
  //  };
    class DS_EXPORT Application
    {
    public:
        Application();
        virtual ~Application();

        void run();
        bool frame();

        virtual void quit();
        virtual void init();
        virtual void handle_event(Event& e);
        virtual void render();
        virtual void update(const TimeStep& dt);
        virtual void imgui_render();
        virtual void debug_draw();
        virtual void handle_new_scene(Scene* scene);

        virtual void serialise();
        virtual void deserialise();
        virtual void load_custom(cereal::JSONInputArchive& input, entt::snapshot_loader& snap_loader) {}
        virtual void save_custom(cereal::JSONOutputArchive& output, const entt::snapshot& snap) {}
        virtual void load_custom(cereal::BinaryInputArchive& input, entt::snapshot_loader& snap_loader) {}
        virtual void save_custom(cereal::BinaryOutputArchive& output, const entt::snapshot& snap) {}

        void    mount_file_system_paths();
        template <typename Archive>
        void save(Archive& archive) const
        {
            int projectVersion = 1;
            archive(cereal::make_nvp("Project Version", projectVersion));
            std::string path;
            archive(cereal::make_nvp("RenderAPI", m_ProjectSettings.RenderAPI),
                cereal::make_nvp("Fullscreen", m_ProjectSettings.Fullscreen),
                cereal::make_nvp("VSync", m_ProjectSettings.VSync),
                cereal::make_nvp("ShowConsole", m_ProjectSettings.ShowConsole),
                cereal::make_nvp("Title", m_ProjectSettings.Title));
            auto paths = m_SceneManager->get_scene_file_paths();
            std::vector<std::string> newPaths;
            for (auto& path : paths)
            {
                std::string newPath;
                FileSystem::get().absolute_path_2_fileSystem(path, newPath);
                newPaths.push_back(path);
            }
            archive(cereal::make_nvp("Scenes", newPaths));
            archive(cereal::make_nvp("SceneIndex", m_SceneManager->get_current_scene_index()));
            archive(cereal::make_nvp("Borderless", m_ProjectSettings.Borderless));
            archive(cereal::make_nvp("EngineAssetPath", m_ProjectSettings.m_EngineAssetPath));
        }
        template <typename Archive>
        void load(Archive& archive)
        {
            int sceneIndex = 0;
            archive(cereal::make_nvp("Project Version", m_ProjectSettings.ProjectVersion));
            archive(cereal::make_nvp("RenderAPI", m_ProjectSettings.RenderAPI),
                cereal::make_nvp("Fullscreen", m_ProjectSettings.Fullscreen),
                cereal::make_nvp("VSync", m_ProjectSettings.VSync),
                cereal::make_nvp("ShowConsole", m_ProjectSettings.ShowConsole),
                cereal::make_nvp("Title", m_ProjectSettings.Title));
            std::vector<std::string> sceneFilePaths;
            archive(cereal::make_nvp("Scenes", sceneFilePaths));

            for (auto& filePath : sceneFilePaths)
            {
                m_SceneManager->add_file_to_load_list(filePath);
            }

            if (sceneFilePaths.size() == sceneIndex)
                add_default_scene();
            archive(cereal::make_nvp("SceneIndex", sceneIndex));
            m_SceneManager->switch_scene(sceneIndex);
            archive(cereal::make_nvp("Borderless", m_ProjectSettings.Borderless));
            archive(cereal::make_nvp("EngineAssetPath", m_ProjectSettings.m_EngineAssetPath));
        }
        void   create_default_project();
        void   add_default_scene();
        ImGuiManager*   get_imgui_manager() const { return m_ImGuiManager.get(); }
        SceneManager*   get_scene_manager() const { return m_SceneManager.get(); }
        DeferedRenderer* get_renderer() const { return m_Renderer.get(); }

        void    set_app_state(AppState state) { m_CurrentState = state; }
        void    set_editor_state(EditorState state) { m_EditorState = state; }
        void    set_scene_active(bool active) { m_SceneActive = active; }
        Scene*  get_current_scene() const;

        bool    get_scene_active() const { return m_SceneActive; }
        Window* get_window() const { return m_Window.get(); }
        AppState get_state() const { return m_CurrentState; }
        EditorState get_editor_state() const { return m_EditorState; }

        std::array<u32,2> get_window_size() const;
        float get_window_dpi() const;

        std::tuple<u32,u32> get_scene_view_dimensions() {return { m_SceneViewWidth ,m_SceneViewHeight }; }
        void set_scene_view_dimensions(uint32_t width, uint32_t height)
        {
            if(width != m_SceneViewWidth)
            {
                m_SceneViewWidth       = width;
                m_SceneViewSizeUpdated = true;
            }

            if(height != m_SceneViewHeight)
            {
                m_SceneViewHeight      = height;
                m_SceneViewSizeUpdated = true;
            }
        }

        
        static Application& get() { return *s_Instance; }

        static void update_systems();
        static void release()
        {
            if (s_Instance)
                delete s_Instance;
            s_Instance = nullptr;
        }

        template <typename Func>
        void queue_event(Func&& func)
        {
            m_EventQueue.push(func);
        }

        template <typename TEvent, bool Immediate = false, typename... TEventArgs>
        void dispatch_event(TEventArgs&&... args)
        {
            SharedPtr<TEvent> event = createSharedPtr<TEvent>(std::forward<TEventArgs>(args)...);
            if (Immediate)
            {
                handle_event(*event);
            }
            else
            {
                std::scoped_lock<std::mutex> lock(m_EventQueueMutex);
                m_EventQueue.push([event]()
                    { Application::get().handle_event(*event); });
            }
        }
        bool handle_window_resize(WindowResizeEvent& e);
        bool handle_renderer_resize(u32 width,u32 height);
        std::shared_ptr<rhi::GpuTexture>    get_main_render_texture();
        void                                set_main_render_texture(std::shared_ptr<rhi::GpuTexture> texture);
        void                                set_override_camera(Camera* camera, maths::Transform* overrideCameraTransform);

        int                                 frame_number() const;
        void                                refresh_shaders();
        void                                invalidate_pt_state();
        struct ProjectSettings
        {
            std::string m_ProjectRoot;
            std::string m_ProjectName;
            std::string m_EngineAssetPath;
            uint32_t Width = 1200, Height = 800;
            bool Fullscreen = true;
            bool VSync = true;
            bool Borderless = false;
            bool ShowConsole = true;
            std::string Title;
            int RenderAPI;
            int ProjectVersion;
            int8_t DesiredGPUIndex = -1;
            std::string IconPath;
            bool DefaultIcon = true;
            bool HideTitleBar = false;
        };
        ProjectSettings& get_project_settings() { return m_ProjectSettings; }
        void open_new_project(const std::string& path, const std::string& name = "New Project",bool create_scene = false);
        void open_project(const std::string& filePath);

        Arena* get_frame_arena() const { return m_FrameArena; }
    protected:
        bool handle_window_close(WindowCloseEvent& e);
        ProjectSettings m_ProjectSettings;
        bool m_ProjectLoaded = false;
        bool m_SceneSaveOnClose = false;
        uint32_t m_Frames = 0;
        uint32_t m_Updates = 0;
        float m_SecondTimer = 0.0f;
        bool m_Minimized = false;

        uint32_t m_SceneViewWidth = 0;
        uint32_t m_SceneViewHeight = 0;
        bool m_SceneViewSizeUpdated = false;
        bool m_SceneActive = true;

        bool m_RenderDocEnabled = false;

        std::mutex m_EventQueueMutex;
        std::queue<std::function<void()>> m_EventQueue;

        UniquePtr<Window> m_Window;
        UniquePtr<Timer> m_Timer;
        UniquePtr<ImGuiManager> m_ImGuiManager;
        UniquePtr<DeferedRenderer> m_Renderer;
        UniquePtr<SceneManager> m_SceneManager;
        //UniquePtr<SystemManager> m_SystemManager;

        static Application* s_Instance;

        std::thread m_UpdateThread;

        std::vector<std::function<void()>> m_MainThreadQueue;
        std::mutex m_MainThreadQueueMutex;

        AppState m_CurrentState = AppState::Loading;
        EditorState m_EditorState = EditorState::Preview;
        AppType m_AppType = AppType::Editor;
        
        Arena* m_FrameArena;
        Arena* m_Arena;
        NONCOPYABLE(Application)
    };

    // Defined by client
    Application* createApplication();
}
