#pragma once
#include "editor_panel.h"
#include "file_browser_panel.h"

#include <maths/ray.h>
#include <maths/transform.h>
#include <utility/ini_file.h>
#include <scene/camera/editor_camera.h>
#include <scene/camera/camera.h>
#include <imgui/imgui_helper.h>
#include <engine/application.h>
#include <maths/rect.h>
#include <imgui/imgui.h>
#include <scene/entity.h>

namespace diverse
{ 

#define BIND_FILEBROWSER_FN(fn) [this](auto&&... args) -> decltype(auto) { \
    return this->fn(std::forward<decltype(args)>(args)...);                \
}


    class Scene;
    class Event;
    class WindowCloseEvent;
    class WindowResizeEvent;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class WindowFileEvent;
    class TimeStep;
    class HelperPanel;

    enum EditorDebugFlags : u32
    {
        Grid = 1,
        Gizmo = 2,
        ViewSelected = 4,
        CameraFrustum = 8,
        MeshBoundingBoxes = 16,
        SpriteBoxes = 32,
        EntityNames = 64,

    };
    namespace asset
    {
        struct Texture;
    }
    class Editor : public Application
    {
        friend class Application;
        friend class SceneViewPanel;
    public:
        explicit Editor();

        virtual ~Editor();


        void init() override;
        void imgui_render() override;
        void render() override;
        void debug_draw() override;
        void handle_event(Event& e) override;
        void quit() override;
        void handle_new_scene(Scene* scene) override;

        void draw_menu_bar();
        void begin_dock_space(bool gameFullScreen);
        void end_dock_space();
        
        void imguizmo(ImDrawList* drawList,
            const ImVec2& cameraPos,
            const ImVec2& windowPos,
            const ImVec2& canvasSize);
        void update(const TimeStep& ts) override;
        void update_gaussian(const TimeStep& ts);

        void load_custom(cereal::JSONInputArchive& input, entt::snapshot_loader& snap_loader) override;
        void save_custom(cereal::JSONOutputArchive& output,const entt::snapshot& snap_loader) override;
        void load_custom(cereal::BinaryInputArchive& input, entt::snapshot_loader& snap_loader) override;
        void save_custom(cereal::BinaryOutputArchive& output, const entt::snapshot& snap) override;

        void create_gaussian_dialog(const std::vector<std::string>& file_path);
        void load_model_dialog(const std::string& filepath);
        void export_model_dialog();
        void set_imguizmo_operation(uint32_t operation)
        {
            im_guizmo_operation = operation;
        }
        uint32_t get_imguizmo_operation() const
        {
            return im_guizmo_operation;
        }
        bool& snap_guizmo()
        {
            return settings.snap_quizmo;
        }
        float& snap_amount()
        {
            return settings.snap_amount;
        }

        void clear_selected()
        {
            selected_entities.clear();
        }

        void set_selected(entt::entity entity);
        void un_select(entt::entity entity);
        void set_hovered_entity(entt::entity entity) { hovered_entity = entity; }
        entt::entity get_hovered_entity() { return hovered_entity; }
        void focus_camera(const glm::vec3& point, float distance, float speed);
        const std::vector<entt::entity>& get_selected() const
        {
            return selected_entities;
        }

        bool is_selected(entt::entity entity)
        {
            if (std::find(selected_entities.begin(), selected_entities.end(), entity) != selected_entities.end())
                return true;

            return false;
        }

        bool is_copied(entt::entity entity)
        {
            if (std::find(copied_entities.begin(), copied_entities.end(), entity) != copied_entities.end())
                return true;

            return false;
        }

        void set_copied_entity(entt::entity entity, bool cut = false)
        {
            if(entity == entt::null) return;
            if (std::find(copied_entities.begin(), copied_entities.end(), entity) != copied_entities.end())
                return;

            copied_entities.push_back(entity);
            cut_copy_entity = cut;
        }

        const std::vector<entt::entity>& get_copied_entity() const
        {
            return copied_entities;
        }

        bool get_cut_copy_entity()
        {
            return cut_copy_entity;
        }

        static Editor* get_editor() { return s_Editor; }
        
        void add_default_editor_settings();
        void save_editor_settings();
        void load_editor_settings();

        glm::vec2  scene_view_panel_position;
        maths::Ray get_screen_ray(int x, int y, Camera* camera, int width, int height);

        void draw_2dgrid(ImDrawList* drawList, const ImVec2& cameraPos, const ImVec2& windowPos, const ImVec2& canvasSize, const float factor, const float thickness);
        void draw_3dgrid();

        bool& show_grid()
        {
            return settings.show_grid;
        }
        const float& get_drid_size() const
        {
            return settings.grid_size;
        }

        bool& show_gizmos()
        {
            return settings.show_gizmos;
        }
        bool& show_view_selected()
        {
            return settings.show_view_selected;
        }

        void toggle_snap()
        {
            settings.snap_quizmo = !settings.snap_quizmo;
        }


        FileBrowserPanel& get_file_browser_panel()
        {
           return file_browser_panel;
        }

        Camera* get_camera() const
        {
            return editor_camera.get();
        }

        EditorCameraController& get_editor_camera_controller()
        {
            return editor_camera_controller;
        }

        maths::Transform& get_editor_camera_transform()
        {
            return editor_camera_transform;
        }

        struct EditorSettings
        {
            float grid_size = 10.0f;
            u32   debug_draw_flags = 0;

            bool show_grid = true;
            bool show_gizmos = true;
            bool show_view_selected = false;
            bool snap_quizmo = false;
            bool shiow_imgui_demo = true;
            bool view_2d = false;
            bool full_screen_on_play = false;
            float snap_amount = 1.0f;
            bool sleep_outof_focus = true;
            float imguizmo_scale = 0.15f;

            bool full_screen_scene_view = false;
            ImGuiHelper::Theme theme = ImGuiHelper::Theme::Black;
            bool free_aspect = true;
            float fixed_aspect = 1.0f;
            bool half_res = false;
            float aspect_ratio = 1.0f;

            // Camera Settings
            float camera_speed = 100.0f;
            float camera_near = 0.01f;
            float camera_far = 1000.0f;

            std::vector<std::string> m_RecentProjects;
        };
        EditorSettings& get_settings() { return settings; }
        void set_scene_view_active(bool active) { scene_view_active = active; }
        bool get_scene_view_active() {return scene_view_active;}
        void set_scene_view_rect(const maths::Rect& rect) { scene_view_rect = rect; }
        std::unordered_map<size_t, const char*>& get_component_iconmap()
        {
            return m_ComponentIconMap;
        }

        bool handle_file_drop(WindowFileEvent& e);
        bool handle_mouse_move(MouseMovedEvent& e);
        bool handle_mouse_pressed(MouseButtonPressedEvent& e);
        bool handle_mouse_released(MouseButtonReleasedEvent& e);
        void file_open_callback(const std::string& filepath);
        void project_open_callback(const std::string& filepath);
        void new_project_open_callback(const std::string& filePath);
        void select_object(const maths::Ray& ray, bool hoveredOnly = false);
        auto get_current_splat_entt()-> entt::entity{return current_splat_entity;}
        bool is_editing_splat();
        
        bool is_gaussian_file(const std::string& filePath);
        bool is_image_file(const std::string& filePath);
        bool is_video_file(const std::string& filePath);
        bool is_hdr_file(const std::string& filePath);
        bool is_project_file(const std::string& filePath);
        bool is_text_file(const std::string& fielPath);
        bool is_font_file(const std::string& path);
        bool is_audio_file(const std::string& filepath);
        bool is_scene_file(const std::string& path);
        bool is_shader_file(const std::string& filepath);
        bool is_mesh_model_file(const std::string& filepath);
        bool is_camera_json_file(const std::string& filepath);
        bool is_texture_paint_active() const;
        void open_file();
        const char* get_iconfont_icon(const std::string& fileType);
        int&  get_current_train_view_id() { return current_train_view_id; }
        auto  get_current_train_view_image()-> std::shared_ptr<asset::Texture>;
        auto  get_hovered_train_view()-> std::shared_ptr<asset::Texture>;
        void  set_hovered_train_view(int id) {hovered_train_view_id = id;}
        SharedPtr<EditorPanel>                      histogram_panel;
        SharedPtr<class TexturePaintPanel>          texture_paint_Panel;
        SharedPtr<class ScenePreviewPanel>          cameras_panel;

        bool& splat_edit() {return is_splat_edit;}
        class Pivot* get_pivot() {return pivot.get();}
        void    merge_splats();
        void    merge_select_splats();
        void    export_mesh();
    public:
        std::vector<std::string>    dropped_files;
        bool                    enable_focus_region = false;
        maths::BoundingBox      get_focus_splat_region();
        maths::Transform        focus_region_transform;
    protected:
        void export_webview();
    #ifdef DS_SPLAT_TRAIN
        void export_cameras();
        void export_sparse_pointcloud();
    #endif
    protected:
        NONCOPYABLE(Editor)

        Application* m_Application;

        uint32_t im_guizmo_operation = 14463;
        std::vector<entt::entity> selected_entities;
        std::vector<entt::entity> copied_entities;
        entt::entity hovered_entity = entt::null;
        entt::entity current_splat_entity = entt::null;
        bool cut_copy_entity = false;

        EditorSettings settings;
        std::vector<SharedPtr<EditorPanel>> panels;
        SharedPtr<HelperPanel>          helper_panels;
        SharedPtr<EditorPanel>          image_2d_panel;  
        std::unordered_map<size_t, const char*> m_ComponentIconMap;
        FileBrowserPanel file_browser_panel;
        
        Camera* current_camera = nullptr;
        EditorCameraController editor_camera_controller;
        maths::Transform editor_camera_transform;
        float camera_transition_speed = 0.0f;
        bool is_transitioning_camera = false;
        glm::vec3 camera_destination;

        SharedPtr<Camera> editor_camera = nullptr;

        std::string temp_scene_save_file_path;
        int auto_save_settings_time = 360000;
        bool scene_view_active = false;
        bool is_new_project_popup_open = false;

        maths::Rect scene_view_rect;
        IniFile ini_file;
        static Editor* s_Editor;

        bool is_open_newgaussian_popup = false;
        bool is_load_model_popup = false;
        bool is_export_model_popup = false;
        bool is_load_mesh = false;
        bool is_load_point = false;
        bool is_export_mesh = false;
        std::vector<std::string> splat_source_path;
        std::string     load_model_path;
        bool        is_train_gaussian = false;
        bool        is_splat_edit = false;
        int         current_train_view_id = -1;
        int         hovered_train_view_id = -1;
        std::shared_ptr<class Pivot> pivot;
    };

}