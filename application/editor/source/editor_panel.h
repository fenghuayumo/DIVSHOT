#pragma once
#include <string>

namespace diverse
{
    class Editor;
    class Scene;
    class MouseMovedEvent;
    class MouseButtonPressedEvent;
    class MouseButtonReleasedEvent;
    class WindowFileEvent;
    class EditorPanel
    {
    public:
        EditorPanel(bool active = true)
            : is_active(active)
        {
        }
        virtual ~EditorPanel() = default;

        const std::string& get_name() const
        {
            return name;
        }
        const std::string& get_simple_name() const
        {
            return simple_name;
        }

        void set_editor(Editor* editor)
        {
            this->editor = editor;
        }
        Editor* get_editor()
        {
            return editor;
        }
        bool& active()
        {
            return is_active;
        }
        void set_active(bool active)
        {
            this->is_active = active;
        }
        virtual void on_imgui_render() = 0;
        virtual void on_new_scene(Scene* scene)
        {
        }

        virtual void on_new_project()
        {
        }

        virtual void on_render()
        {
        }

        virtual void destroy_graphics_resources()
        {
        }
        virtual void on_update(float dt)
        {
        }
        virtual bool handle_mouse_move(MouseMovedEvent& e) {return true;}
        virtual bool handle_mouse_pressed(MouseButtonPressedEvent& e) {return true;}
        virtual bool handle_mouse_released(MouseButtonReleasedEvent& e) {return true;}
        virtual bool handle_file_drop(WindowFileEvent& e) {return true;} 
    protected:
        bool is_active = true;
        std::string name;
        std::string simple_name;
        Editor* editor = nullptr;
    };
}
