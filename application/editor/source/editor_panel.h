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
            : m_Active(active)
        {
        }
        virtual ~EditorPanel() = default;

        const std::string& get_name() const
        {
            return m_Name;
        }
        const std::string& get_simple_name() const
        {
            return m_SimpleName;
        }

        void set_editor(Editor* editor)
        {
            m_Editor = editor;
        }
        Editor* get_editor()
        {
            return m_Editor;
        }
        bool& active()
        {
            return m_Active;
        }
        void set_active(bool active)
        {
            m_Active = active;
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
        bool m_Active = true;
        std::string m_Name;
        std::string m_SimpleName;
        Editor* m_Editor = nullptr;
    };
}
