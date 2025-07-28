
#include <map>
#include "renderer/ui_renderer.h"
#include "core/core.h"
#include "core/profiler.h"
#include "engine/input.h"
#include "engine/window.h"
#include "engine/application.h"

#include "imgui_manager.h"
#include "imgui_renderer.h"
#include "engine/file_system.h"

#include "imgui_helper.h"
#include "maths/maths_utils.h"
#include "IconsMaterialDesignIcons.h"

#include <imgui/imgui.h>
#include <imgui/Plugins/ImGuizmo.h>
#include <imgui/Plugins/ImGuiAl/fonts/MaterialDesign.inl>
#include <imgui/Plugins/ImGuiAl/fonts/RobotoMedium.inl>
#include <imgui/Plugins/ImGuiAl/fonts/RobotoRegular.inl>
#include <imgui/Plugins/ImGuiAl/fonts/RobotoBold.inl>
#include <imgui/misc/freetype/imgui_freetype.h>

#if defined(DS_PLATFORM_MACOS) || defined(DS_PLATFORM_WINDOWS) || defined(DS_PLATFORM_LINUX)
#define USING_GLFW
#endif

#ifdef USING_GLFW
#include <GLFW/glfw3.h>
#endif


#include "core/ds_log.h"

namespace diverse
{
    ImGuiManager::ImGuiManager(rhi::GpuDevice* device, rhi::Swapchain* swapchain,UiRenderer* ui_renderer)
        : m_UIRender(ui_renderer)
    {
        m_FontSize = 14.0f;

#ifdef DS_PLATFORM_IOS
        m_FontSize *= 2.0f;
#endif
        m_IMGUIRenderer = UniquePtr<IMGUIRenderer>(IMGUIRenderer::create(device, swapchain));
    }

    ImGuiManager::~ImGuiManager()
    {
    }

#ifdef USING_GLFW
    static const char* ImGui_ImplGlfw_GetClipboardText(void*)
    {
        return glfwGetClipboardString((GLFWwindow*)Application::get().get_window()->get_handle());
    }

    static void ImGui_ImplGlfw_SetClipboardText(void*, const char* text)
    {
        glfwSetClipboardString((GLFWwindow*)Application::get().get_window()->get_handle(), text);
    }
#endif

    void ImGuiManager::init()
    {
        DS_PROFILE_FUNCTION();

        DS_LOG_INFO("ImGui Version : {0}", IMGUI_VERSION);
#ifdef IMGUI_USER_CONFIG
        DS_LOG_INFO("ImConfig File : {0}", std::string(IMGUI_USER_CONFIG));
#endif
        Application& app = Application::get();
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(app.get_window()->get_width()), static_cast<float>(app.get_window()->get_height()));
        // io.DisplayFramebufferScale = ImVec2(app.get_window()->GetDPIScale(), app.get_window()->GetDPIScale());
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        m_DPIScale = app.get_window()->get_dpi_scale();
#ifdef DS_PLATFORM_IOS
        io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
#endif
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        m_FontSize *= app.get_window()->get_dpi_scale();

        set_imgui_key_codes();
        set_imgui_style();

#ifdef DS_PLATFORM_IOS
        ImGui::GetStyle().ScaleAllSizes(1.5f);
        ImGuiStyle& style = ImGui::GetStyle();

        style.ScrollbarSize = 20;
#endif
#ifdef DS_PLATFORM_MACOS
        ImGui::GetStyle().ScaleAllSizes(m_DPIScale);
#endif

        if (m_IMGUIRenderer)
            m_IMGUIRenderer->init();

#ifdef USING_GLFW
        io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
        io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
#endif
    }

    void ImGuiManager::handle_event(Event& event)
    {
        DS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(ImGuiManager::handle_mouse_button_pressed_event));
        dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(ImGuiManager::handle_mouse_mutton_released_event));
        dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(ImGuiManager::handle_mouse_moved_event));
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(ImGuiManager::handle_mouse_scrolled_event));
        dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(ImGuiManager::handle_key_pressed_event));
        dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(ImGuiManager::handle_key_released_event));
        dispatcher.Dispatch<KeyTypedEvent>(BIND_EVENT_FN(ImGuiManager::handle_key_typed_event));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(ImGuiManager::handle_window_resize_event));
    }

    void ImGuiManager::render(std::function<void()> callback)
    {
        DS_PROFILE_FUNCTION();
        ImGuizmo::BeginFrame();

        if (m_IMGUIRenderer )
        {
            m_IMGUIRenderer->new_frame();

            callback();

            m_UIRender->ui_frame = std::make_pair< UiRenderCallback, std::shared_ptr<rhi::GpuTexture>>([&](rhi::CommandBuffer* cmd_buf){
                m_IMGUIRenderer->render(cmd_buf);
            }, m_IMGUIRenderer->get_target_tex());
        }
    }

    void ImGuiManager::handle_new_scene(Scene* scene)
    {
        DS_PROFILE_FUNCTION();
        m_IMGUIRenderer->clear();
    }

    int diverseMouseButtonToImGui(diverse::InputCode::MouseKey key)
    {
        switch (key)
        {
        case diverse::InputCode::MouseKey::ButtonLeft:
            return 0;
        case diverse::InputCode::MouseKey::ButtonRight:
            return 1;
        case diverse::InputCode::MouseKey::ButtonMiddle:
            return 2;
        default:
            return 4;
        }

        return 4;
    }

    bool ImGuiManager::handle_mouse_button_pressed_event(MouseButtonPressedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[diverseMouseButtonToImGui(e.GetMouseButton())] = true;

        return false;
    }

    bool ImGuiManager::handle_mouse_mutton_released_event(MouseButtonReleasedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[diverseMouseButtonToImGui(e.GetMouseButton())] = false;

        return false;
    }

    bool ImGuiManager::handle_mouse_moved_event(MouseMovedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (Input::get().get_mouse_mode() == MouseMode::Visible)
            io.MousePos = ImVec2(e.GetX() * m_DPIScale, e.GetY() * m_DPIScale);

        return false;
    }

    bool ImGuiManager::handle_mouse_scrolled_event(MouseScrolledEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent((float)e.GetXOffset(), (float)e.GetYOffset());

        return false;
    }

    bool ImGuiManager::handle_key_pressed_event(KeyPressedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[(int)e.GetKeyCode()] = true;

        io.KeyCtrl = io.KeysDown[(int)diverse::InputCode::Key::LeftControl] || io.KeysDown[(int)diverse::InputCode::Key::RightControl];
        io.KeyShift = io.KeysDown[(int)diverse::InputCode::Key::LeftShift] || io.KeysDown[(int)diverse::InputCode::Key::RightShift];
        io.KeyAlt = io.KeysDown[(int)diverse::InputCode::Key::LeftAlt] || io.KeysDown[(int)diverse::InputCode::Key::RightAlt];

#ifdef _WIN32
        io.KeySuper = false;
#else
        io.KeySuper = io.KeysDown[(int)diverse::InputCode::Key::LeftSuper] || io.KeysDown[(int)diverse::InputCode::Key::RightSuper];
#endif

        return io.WantTextInput;
    }

    bool ImGuiManager::handle_key_released_event(KeyReleasedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.KeysDown[(int)e.GetKeyCode()] = false;

        return false;
    }

    bool ImGuiManager::handle_key_typed_event(KeyTypedEvent& e)
    {
        ImGuiIO& io = ImGui::GetIO();
        int keycode = (int)e.Character;
        if (keycode > 0 && keycode < 0x10000)
            io.AddInputCharacter((unsigned short)keycode);

        return false;
    }

    bool ImGuiManager::handle_window_resize_event(WindowResizeEvent& e)
    {
        DS_PROFILE_FUNCTION();
        ImGuiIO& io = ImGui::GetIO();

        uint32_t width = maths::Max(1u, e.GetWidth());
        uint32_t height = maths::Max(1u, e.GetHeight());

        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        // io.DisplayFramebufferScale = ImVec2(e.GetDPIScale(), e.GetDPIScale());
        m_DPIScale = e.GetDPIScale();
        m_IMGUIRenderer->handle_resize(width, height);

        return false;
    }

    void ImGuiManager::set_imgui_key_codes()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
        io.KeyMap[ImGuiKey_Tab] = (int)diverse::InputCode::Key::Tab;
        io.KeyMap[ImGuiKey_LeftArrow] = (int)diverse::InputCode::Key::Left;
        io.KeyMap[ImGuiKey_RightArrow] = (int)diverse::InputCode::Key::Right;
        io.KeyMap[ImGuiKey_UpArrow] = (int)diverse::InputCode::Key::Up;
        io.KeyMap[ImGuiKey_DownArrow] = (int)diverse::InputCode::Key::Down;
        io.KeyMap[ImGuiKey_PageUp] = (int)diverse::InputCode::Key::PageUp;
        io.KeyMap[ImGuiKey_PageDown] = (int)diverse::InputCode::Key::PageDown;
        io.KeyMap[ImGuiKey_Home] = (int)diverse::InputCode::Key::Home;
        io.KeyMap[ImGuiKey_End] = (int)diverse::InputCode::Key::End;
        io.KeyMap[ImGuiKey_Insert] = (int)diverse::InputCode::Key::Insert;
        io.KeyMap[ImGuiKey_Delete] = (int)diverse::InputCode::Key::Delete;
        io.KeyMap[ImGuiKey_Backspace] = (int)diverse::InputCode::Key::Backspace;
        io.KeyMap[ImGuiKey_Space] = (int)diverse::InputCode::Key::Space;
        io.KeyMap[ImGuiKey_Enter] = (int)diverse::InputCode::Key::Enter;
        io.KeyMap[ImGuiKey_Escape] = (int)diverse::InputCode::Key::Escape;
        io.KeyMap[ImGuiKey_A] = (int)diverse::InputCode::Key::A;
        io.KeyMap[ImGuiKey_C] = (int)diverse::InputCode::Key::C;
        io.KeyMap[ImGuiKey_V] = (int)diverse::InputCode::Key::V;
        io.KeyMap[ImGuiKey_X] = (int)diverse::InputCode::Key::X;
        io.KeyMap[ImGuiKey_Y] = (int)diverse::InputCode::Key::Y;
        io.KeyMap[ImGuiKey_Z] = (int)diverse::InputCode::Key::Z;
        io.KeyRepeatDelay = 0.400f;
        io.KeyRepeatRate = 0.05f;
    }

    void ImGuiManager::set_imgui_style()
    {
        DS_PROFILE_FUNCTION();
        ImGuiIO& io = ImGui::GetIO();

        ImGui::StyleColorsDark();

        io.FontGlobalScale = 1.0f;

        ImFontConfig icons_config;
        icons_config.MergeMode = false;
        icons_config.PixelSnapH = true;
        icons_config.OversampleH = icons_config.OversampleV = 1;
        icons_config.GlyphMinAdvanceX = 4.0f;
        icons_config.SizePixels = 12.0f;

        static const ImWchar ranges[] = {
            0x0020,
            0x00FF,
            0x0400,
            0x044F,
            0,
        };

        io.Fonts->AddFontFromMemoryCompressedTTF(RobotoRegular_compressed_data, RobotoRegular_compressed_size, m_FontSize, &icons_config, ranges);
        add_icon_font();

        io.Fonts->AddFontFromMemoryCompressedTTF(RobotoBold_compressed_data, RobotoBold_compressed_size, m_FontSize + 2.0f, &icons_config, ranges);

        io.Fonts->AddFontFromMemoryCompressedTTF(RobotoRegular_compressed_data, RobotoRegular_compressed_size, m_FontSize * 0.8f, &icons_config, ranges);
        // AddIconFont();
        //io.Fonts->AddFontDefault();
       io.Fonts->AddFontFromFileTTF("../resource/fonts/simkai.ttf", m_FontSize, &icons_config, io.Fonts->GetGlyphRangesChineseFull());

        io.Fonts->TexGlyphPadding = 1;
        for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
        {
            ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
            font_config->RasterizerMultiply = 1.0f;
        }

        ImGuiStyle& style = ImGui::GetStyle();
        style.AntiAliasedFill = true;
        style.AntiAliasedLines = true;

        style.WindowPadding = ImVec2(5, 5);
        style.FramePadding = ImVec2(4, 4);
        style.ItemSpacing = ImVec2(6, 2);
        style.ItemInnerSpacing = ImVec2(2, 2);
        style.IndentSpacing = 6.0f;
        style.TouchExtraPadding = ImVec2(4, 4);

        style.ScrollbarSize = 10;

        style.WindowBorderSize = 0;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 3;
        style.FrameBorderSize = 0.0f;

        const int roundingAmount = 2;
        style.PopupRounding = roundingAmount;
        style.WindowRounding = roundingAmount;
        style.ChildRounding = 0;
        style.FrameRounding = roundingAmount;
        style.ScrollbarRounding = roundingAmount;
        style.GrabRounding = roundingAmount;
        style.WindowMinSize = ImVec2(200.0f, 100.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

#ifdef IMGUI_HAS_DOCK
        style.TabBorderSize = 1.0f;
        style.TabRounding = roundingAmount; // + 4;

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = roundingAmount;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
#endif

        ImGuiHelper::SetTheme(ImGuiHelper::Theme::Dark);
    }

    void ImGuiManager::add_icon_font()
    {
        ImGuiIO& io = ImGui::GetIO();

        static const ImWchar icons_ranges[] = { ICON_MIN_MDI, ICON_MAX_MDI, 0 };
        ImFontConfig icons_config;
        // merge in icons from Font Awesome
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphOffset.y = 1.0f;
        icons_config.OversampleH = icons_config.OversampleV = 1;
        icons_config.GlyphMinAdvanceX = 4.0f;
        icons_config.SizePixels = 12.0f;

        io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data, MaterialDesign_compressed_size, m_FontSize, &icons_config, icons_ranges);
    }

    ImGuiTextureID* add_imgui_texture(const std::shared_ptr<rhi::GpuTexture>& tex)
    {
        return Application::get().get_imgui_manager()->get_imgui_renderer()->add_texture(tex);
    }

}
