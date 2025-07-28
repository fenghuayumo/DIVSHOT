#pragma once
#include "core/core.h"
#include "core/reference.h"
#include "events/key_event.h"
#include "events/event.h"
#include "events/mouse_event.h"
#include "events/application_event.h"


namespace diverse
{
    class Scene;
    class TimeStep;

    class IMGUIRenderer;

    namespace rhi
    {
        struct GpuDevice;
        struct Swapchain;
        struct GpuTexture;
    }
    class DS_EXPORT ImGuiManager
    {
    public:
        ImGuiManager(rhi::GpuDevice* device, rhi::Swapchain* swapchain, struct UiRenderer*);
        ~ImGuiManager();

        void init();
 
        void handle_event(Event& event);
        void render(std::function<void()> callback);
        void handle_new_scene(Scene* scene);

        IMGUIRenderer* get_imgui_renderer() const { return m_IMGUIRenderer.get(); }

    private:
        bool handle_mouse_button_pressed_event(MouseButtonPressedEvent& e);
        bool handle_mouse_mutton_released_event(MouseButtonReleasedEvent& e);
        bool handle_mouse_moved_event(MouseMovedEvent& e);
        bool handle_mouse_scrolled_event(MouseScrolledEvent& e);
        bool handle_key_pressed_event(KeyPressedEvent& e);
        bool handle_key_released_event(KeyReleasedEvent& e);
        bool handle_key_typed_event(KeyTypedEvent& e);
        bool handle_window_resize_event(WindowResizeEvent& e);

        void set_imgui_key_codes();
        void set_imgui_style();
        void add_icon_font();

        float m_FontSize;
        float m_DPIScale;

        UniquePtr<IMGUIRenderer> m_IMGUIRenderer;
        bool m_ClearScreen = false;
        struct UiRenderer*   m_UIRender;
    };
}