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
            return m_Data.Title;
        }
        inline uint32_t get_width() const override
        {
            return m_Data.Width;
        }
        inline uint32_t get_height() const override
        {
            return m_Data.Height;
        }
        inline float get_screen_ratio() const override
        {
            return (float)m_Data.Width / (float)m_Data.Height;
        }
        inline bool get_exit() const override
        {
            return m_Data.Exit;
        }
        inline void set_exit(bool exit) override
        {
            m_Data.Exit = exit;
        }
        inline void set_event_callback(const EventCallbackFn& callback) override
        {
            m_Data.EventCallback = callback;
        }

        inline void* get_handle() override
        {
            return hWnd;
        }
        std::array<u32,2> get_frame_buffer_size() const override {return {get_width(), get_height()};}
        struct WindowData
        {
            std::string Title;
            uint32_t Width = 0, Height = 0;
            bool VSync;
            bool Exit;
            RenderAPI m_RenderAPI;

            EventCallbackFn EventCallback;
        };

        WindowData m_Data;

        HINSTANCE GetHInstance() const
        {
            return hInstance;
        }
        HWND GetHWND() const
        {
            return hWnd;
        }

        static void MakeDefault();

    protected:
        static Window* CreateFuncWindows(const WindowDesc& properties);

        HINSTANCE hInstance {};
        HDC hDc {};
        HWND hWnd;
        RAWINPUTDEVICE rid {};

        HICON m_BigIcon;
        HICON m_SmallIcon;
    };

}