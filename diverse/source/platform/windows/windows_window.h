#pragma once

#include "engine/window.h"
#include <Windows.h>

namespace diverse
{
    enum class RenderAPI : uint32_t;

    class DS_EXPORT WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowDesc& properties);
        ~WindowsWindow();

        void toggle_vsync() override;
        void set_window_title(const std::string& title) override;
        void set_borderless_window(bool borderless) override;
        void on_update() override;
        void process_input() override;

        void set_vsync(bool set) override {};
        void hide_mouse(bool hide) override {};
        void set_mouse_position(const glm::vec2& pos) override {};
        void update_cursor_imgui() override;
        void set_icon(const WindowDesc& desc) override;

        bool init(const WindowDesc& properties);

        inline std::string get_title() const override
        {
            return data.title;
        }
        inline uint32_t get_width() const override
        {
            return data.width;
        }
        inline uint32_t get_height() const override
        {
            return data.height;
        }
        inline float get_screen_ratio() const override
        {
            return (float)data.width / (float)data.height;
        }
        inline bool get_exit() const override
        {
            return data.exit;
        }
        inline void set_exit(bool exit) override
        {
            data.exit = exit;
        }
        inline void set_event_callback(const EventCallbackFn& callback) override
        {
            data.event_callback = callback;
        }

        inline void* get_handle() override
        {
            return hwnd;
        }
        std::array<u32,2> get_frame_buffer_size() const override {return {get_width(), get_height()};}
        
        struct WindowData
        {
            std::string title;
            uint32_t width = 0, height = 0;
            bool vsync;
            bool exit;
            RenderAPI render_api;

            EventCallbackFn event_callback;
        };

        WindowData data;

        HINSTANCE get_hinstance() const
        {
            return hinstance;
        }
        HWND get_hwnd() const
        {
            return hwnd;
        }

        static void make_default();

    protected:
        static Window* create_func_windows(const WindowDesc& properties);

        HINSTANCE hinstance {};
        HDC hdc {};
        HWND hwnd;
        RAWINPUTDEVICE rid {};

        HICON big_icon;
        HICON small_icon;
    };

}