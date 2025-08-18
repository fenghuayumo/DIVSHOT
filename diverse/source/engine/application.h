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
            archive(cereal::make_nvp("RenderAPI", project_settings.RenderAPI),
                cereal::make_nvp("Fullscreen", project_settings.Fullscreen),
                cereal::make_nvp("VSync", project_settings.VSync),
                cereal::make_nvp("ShowConsole", project_settings.ShowConsole),
                cereal::make_nvp("Title", project_settings.Title));
            auto paths = scene_manager->get_scene_file_paths();
            std::vector<std::string> newPaths;
            for (auto& path : paths)
            {
                std::string newPath;
                FileSystem::get().absolute_path_2_fileSystem(path, newPath);
                newPaths.push_back(path);
            }
            archive(cereal::make_nvp("Scenes", newPaths));
            archive(cereal::make_nvp("SceneIndex", scene_manager->get_current_scene_index()));
            archive(cereal::make_nvp("Borderless", project_settings.Borderless));
            archive(cereal::make_nvp("EngineAssetPath", project_settings.EngineAssetPath));
        }
        template <typename Archive>
        void load(Archive& archive)
        {
            int sceneIndex = 0;
            archive(cereal::make_nvp("Project Version", project_settings.ProjectVersion));
            archive(cereal::make_nvp("RenderAPI", project_settings.RenderAPI),
                cereal::make_nvp("Fullscreen", project_settings.Fullscreen),
                cereal::make_nvp("VSync", project_settings.VSync),
                cereal::make_nvp("ShowConsole", project_settings.ShowConsole),
                cereal::make_nvp("Title", project_settings.Title));
            std::vector<std::string> sceneFilePaths;
            archive(cereal::make_nvp("Scenes", sceneFilePaths));

            for (auto& filePath : sceneFilePaths)
            {
                scene_manager->add_file_to_load_list(filePath);
            }

            if (sceneFilePaths.size() == sceneIndex)
                add_default_scene();
            archive(cereal::make_nvp("SceneIndex", sceneIndex));
            scene_manager->switch_scene(sceneIndex);
            archive(cereal::make_nvp("Borderless", project_settings.Borderless));
            archive(cereal::make_nvp("EngineAssetPath", project_settings.EngineAssetPath));
        }
        void   create_default_project();
        void   add_default_scene();
        ImGuiManager*   get_imgui_manager() const { return imgui_manager.get(); }
        SceneManager*   get_scene_manager() const { return scene_manager.get(); }
        DeferedRenderer* get_renderer() const { return main_renderer.get(); }

        void    set_app_state(AppState state) { current_state = state; }
        void    set_editor_state(EditorState state) { editor_state = state; }
        void    set_scene_active(bool active) { scene_active = active; }
        Scene*  get_current_scene() const;

        bool    get_scene_active() const { return scene_active; }
        Window* get_window() const { return window.get(); }
        AppState get_state() const { return current_state; }
        EditorState get_editor_state() const { return editor_state; }

        std::array<u32,2> get_window_size() const;
        float get_window_dpi() const;

        std::tuple<u32,u32> get_scene_view_dimensions() {return { scene_view_width ,scene_view_height }; }
        void set_scene_view_dimensions(uint32_t width, uint32_t height)
        {
            if(width != scene_view_width)
            {
                scene_view_width       = width;
                scene_view_size_updated = true;
            }

            if(height != scene_view_height)
            {
                scene_view_height      = height;
                scene_view_size_updated = true;
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
            event_queue.push(func);
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
                std::scoped_lock<std::mutex> lock(event_queue_mutex);
                event_queue.push([event]()
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
            std::string ProjectRoot;
            std::string ProjectName;
            std::string EngineAssetPath;
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
        ProjectSettings& get_project_settings() { return project_settings; }
        void open_new_project(const std::string& path, const std::string& name = "New Project",bool create_scene = false);
        void open_project(const std::string& filePath);

        Arena* get_frame_arena() const { return frame_arena; }
    protected:
        bool handle_window_close(WindowCloseEvent& e);
        ProjectSettings project_settings;
        bool project_loaded = false;
        bool scene_save_on_close = false;
        uint32_t frame_cnts = 0;
        uint32_t update_cnts = 0;
        float second_timer = 0.0f;
        bool  is_minimized = false;

        uint32_t scene_view_width = 0;
        uint32_t scene_view_height = 0;
        bool scene_view_size_updated = false;
        bool scene_active = true;

        bool render_doc_enabled = false;

        std::mutex event_queue_mutex;
        std::queue<std::function<void()>> event_queue;

        UniquePtr<Window>   window;
        UniquePtr<Timer>    timer;
        UniquePtr<ImGuiManager> imgui_manager;
        UniquePtr<DeferedRenderer> main_renderer;
        UniquePtr<SceneManager> scene_manager;
        //UniquePtr<SystemManager> m_SystemManager;

        static Application* s_Instance;

        std::thread update_thread;

        std::vector<std::function<void()>> main_thread_queue;
        std::mutex main_thread_queue_mutex;

        AppState current_state = AppState::Loading;
        EditorState editor_state = EditorState::Preview;
        AppType app_type = AppType::Editor;
        
        Arena* frame_arena;
        Arena* arena;
        NONCOPYABLE(Application)
    };

    // Defined by client
    Application* createApplication();
}
