#pragma once

#include "engine/window.h"

struct GLFWwindow;

namespace diverse
{
    
    enum class RenderAPI : uint32_t;


    class DS_EXPORT GLFWWindow : public Window
    {
    public:
        GLFWWindow(const WindowDesc& properties);
        ~GLFWWindow();

        void toggle_vsync() override;
        void set_vsync(bool set) override;
        void set_window_title(const std::string& title) override;
        void set_borderless_window(bool borderless) override;
        void on_update() override;
        void hide_mouse(bool hide) override;
        void set_mouse_position(const glm::vec2& pos) override;
        void update_cursor_imgui() override;
        void process_input() override;
        void maximise() override;

        bool init(const WindowDesc& properties);
        std::array<u32,2> get_frame_buffer_size() const override;
        
        inline void* get_handle() override
        {
            return handle;
        }

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

        inline float get_dpi_scale() const override
        {
            return data.dpi_scale;
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

        void set_icon(const WindowDesc& desc) override;

        static void make_default();

    protected:
        static Window* create_func_glfw(const WindowDesc& properties);

        GLFWwindow* handle;

        struct WindowData
        {
            std::string title;
            uint32_t width, height;
            bool vsync;
            bool exit;
            RenderAPI render_api;
            float dpi_scale;

            EventCallbackFn event_callback;
        };

        WindowData data;
    };
}
