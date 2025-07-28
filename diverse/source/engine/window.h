#pragma once
#include "core/reference.h"
#include "events/event.h"
#include <string>
#include <glm/vec2.hpp>
#include <glm/fwd.hpp>
#include <vector>
#include <functional>
#include <array>
namespace diverse
{
    struct DS_EXPORT WindowDesc
    {
        WindowDesc(uint32_t width = 1280, uint32_t height = 720, int renderAPI = 0, const std::string& title = "diverse", bool fullscreen = false, bool vSync = true, bool borderless = false, const std::string& filepath = "")
            : Width(width)
            , Height(height)
            , Title(title)
            , Fullscreen(fullscreen)
            , VSync(vSync)
            , Borderless(borderless)
            , RenderAPI(renderAPI)
            , FilePath(filepath)
        {
        }

        uint32_t Width, Height;
        bool Fullscreen;
        bool VSync;
        bool Borderless;
        bool Resizeable = true;
        bool ShowConsole = true;
        std::string Title;
        int RenderAPI;
        std::string FilePath;
        std::vector<std::string> IconPaths;
        std::vector<std::pair<uint32_t, uint8_t*>> IconData;
    };

    class DS_EXPORT Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        static Window* create(const WindowDesc& windowDesc);
        virtual ~Window();
        bool initialise(const WindowDesc& windowDesc);

        bool has_initialised() const
        {
            return m_Init;
        };

        virtual bool get_exit() const    = 0;
        virtual void set_exit(bool exit) = 0;

        inline void set_has_resized(bool resized)
        {
            m_HasResized = resized;
        }
        inline bool get_has_resized() const
        {
            return m_HasResized;
        }

        virtual void toggle_vsync()                            = 0;
        virtual void set_vsync(bool set)                       = 0;
        virtual void set_window_title(const std::string& title) = 0;
        virtual void on_update()                               = 0;
        virtual void process_input() {};
        virtual void set_borderless_window(bool borderless) = 0;
        virtual void* get_handle()
        {
            return nullptr;
        };
        virtual float get_screen_ratio() const = 0;
        virtual void hide_mouse(bool hide) {};
        virtual void set_mouse_position(const glm::vec2& pos) {};
        virtual void set_event_callback(const EventCallbackFn& callback) = 0;
        virtual void update_cursor_imgui()                               = 0;
        virtual void set_icon(const WindowDesc& desc)                   = 0;
        virtual void maximise() {};
        virtual std::string get_title() const = 0;
        virtual uint32_t get_width() const    = 0;
        virtual uint32_t get_height() const   = 0;
        virtual float get_dpi_scale() const { return 1.0f; }
        virtual bool get_vsync() const { return m_VSync; };
        virtual std::array<u32,2> get_frame_buffer_size() const = 0;
        void set_window_focus(bool focus) { m_WindowFocus = focus; }
        bool get_window_focus() const { return m_WindowFocus; }

    protected:
        static Window* (*CreateFunc)(const WindowDesc&);

        Window() = default;

        bool m_Init = false;
        glm::vec2 m_Position;
        bool m_VSync       = false;
        bool m_HasResized  = false;
        bool m_WindowFocus = true;
    };

}
