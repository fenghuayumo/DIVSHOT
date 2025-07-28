#include "precompile.h"
#if defined(DS_PLATFORM_MACOS)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#include "backend/drs_ogl_rhi/GL.h"
#include "backend/drs_rhi/drs_rhi.h"

#include "glfw_window.h"

#include "utility/load_image.h"

#include "glfw_key_codes.h"

#include "engine/os.h"
#include "engine/input.h"
#include "engine/application.h"

#include "events/application_event.h"
#include "events/mouse_event.h"
#include "events/key_event.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui/imgui.h>

static GLFWcursor* g_MouseCursors[ImGuiMouseCursor_COUNT] = { 0 };

namespace diverse
{
    static bool s_GLFWInitialized = false;
    static int s_NumGLFWWindows   = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        DS_LOG_ERROR("GLFW Error - {0} : {1}", error, description);
    }

    GLFWWindow::GLFWWindow(const WindowDesc& properties)
    {
        DS_PROFILE_FUNCTION();
        m_Init  = false;
        m_VSync = properties.VSync;

        DS_LOG_INFO("VSync : {0}", m_VSync ? "True" : "False");
        m_HasResized       = true;
        m_Data.m_RenderAPI = static_cast<RenderAPI>(properties.RenderAPI);
        m_Data.VSync       = m_VSync;
        m_Init             = init(properties);

        // Setting fullscreen overrides width and heigh in Init
        auto propCopy   = properties;
        propCopy.Width  = m_Data.Width;
        propCopy.Height = m_Data.Height;
    }

    GLFWWindow::~GLFWWindow()
    {
        for(ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        {
            glfwDestroyCursor(g_MouseCursors[cursor_n]);
            g_MouseCursors[cursor_n] = NULL;
        }

        glfwDestroyWindow(m_Handle);

        s_NumGLFWWindows--;

        if(s_NumGLFWWindows < 1)
        {
            s_GLFWInitialized = false;
            glfwTerminate();
        }
    }

    bool GLFWWindow::init(const WindowDesc& properties)
    {
        DS_PROFILE_FUNCTION();
        DS_LOG_INFO("Creating window - Title : {0}, Width : {1}, Height : {2}", properties.Title, properties.Width, properties.Height);

        if(!s_GLFWInitialized)
        {
            int success = glfwInit();
            DS_ASSERT(success, "Could not initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);

            s_GLFWInitialized = true;
        }

        s_NumGLFWWindows++;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        float xscale, yscale;
        glfwGetMonitorContentScale(monitor, &xscale, &yscale);
        m_Data.DPIScale = xscale;

#ifdef DS_PLATFORM_MACOS
        if(m_Data.DPIScale > 1.0f)
        {
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
        }
        else
        {
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
            glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
        }
#endif

#ifdef DS_RENDER_API_OPENGL
        if(m_Data.m_RenderAPI == RenderAPI::OPENGL)
        {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef DS_PLATFORM_MACOS
            glfwWindowHint(GLFW_SAMPLES, 1);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
            glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GL_TRUE);
            glfwWindowHint(GLFW_STENCIL_BITS, 8); // Fixes 16 bit stencil bits in macOS.
            glfwWindowHint(GLFW_STEREO, GLFW_FALSE);
            glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
#endif
        }
#endif
        set_borderless_window(properties.Borderless);

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        uint32_t ScreenWidth  = 0;
        uint32_t ScreenHeight = 0;

        if(properties.Fullscreen)
        {
            ScreenWidth  = mode->width;
            ScreenHeight = mode->height;
        }
        else
        {
            ScreenWidth  = properties.Width;
            ScreenHeight = properties.Height;
        }

        m_Data.Title  = properties.Title;
        m_Data.Width  = ScreenWidth;
        m_Data.Height = ScreenHeight;
        m_Data.Exit   = false;

#if defined(DS_RENDER_API_VULKAN) || defined(DS_RENDER_API_METAL)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

        m_Handle = glfwCreateWindow(ScreenWidth, ScreenHeight, properties.Title.c_str(), nullptr, nullptr);

        int w, h;
        glfwGetFramebufferSize(m_Handle, &w, &h);
        m_Data.Width  = w;
        m_Data.Height = h;

#ifdef DS_RENDER_API_OPENGL
        if(m_Data.m_RenderAPI == RenderAPI::OPENGL)
        {
            glfwMakeContextCurrent(m_Handle);

            if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                DS_LOG_ERROR("Failed to initialise OpenGL context");
            }
        }
#endif

        glfwSetWindowUserPointer(m_Handle, &m_Data);

        if(glfwRawMouseMotionSupported())
            glfwSetInputMode(m_Handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        // #ifndef DS_PLATFORM_MACOS
        set_icon(properties);
        // #endif

        // glfwSetWindowPos(m_Handle, mode->width / 2 - m_Data.Width / 2, mode->height / 2 - m_Data.Height / 2);
        glfwSetInputMode(m_Handle, GLFW_STICKY_KEYS, true);

         #ifdef DS_PLATFORM_WINDOWS
        		glfwGetWindowSize(m_Handle, &w, &h);
        		m_Data.DPIScale = (float)w / m_Data.Width;
         #endif

        // Set GLFW callbacks
        glfwSetWindowSizeCallback(m_Handle, [](GLFWwindow* window, int width, int height)
                                  {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));

                int w, h;
                glfwGetFramebufferSize(window, &w, &h);

                data.DPIScale = (float)w / (float)width;

                data.Width = uint32_t(width * data.DPIScale);
                data.Height = uint32_t(height * data.DPIScale);

                WindowResizeEvent event(data.Width, data.Height, data.DPIScale);
                data.EventCallback(event); });

        glfwSetWindowCloseCallback(m_Handle, [](GLFWwindow* window)
                                   {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));
                WindowCloseEvent event;
                data.EventCallback(event);
                data.Exit = true; });

        glfwSetWindowFocusCallback(m_Handle, [](GLFWwindow* window, int focused)
                                   { 
			Window* lmWindow = Application::get().get_window();

			if(lmWindow)
				lmWindow->set_window_focus(focused); });

        glfwSetWindowIconifyCallback(m_Handle, [](GLFWwindow* window, int32_t state)
                                     {
                switch(state)
                {
                case GL_TRUE:
                    Application::get().get_window()->set_window_focus(false);
                    break;
                case GL_FALSE:
                    Application::get().get_window()->set_window_focus(true);
                    break;
                default:
                    DS_LOG_INFO("Unsupported window iconify state from callback");
                } });

        glfwSetKeyCallback(m_Handle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
                           {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));

                switch(action)
                {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(GLFWKeyCodes::glfw2diverseKeyboardKey(key), 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(GLFWKeyCodes::glfw2diverseKeyboardKey(key));
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(GLFWKeyCodes::glfw2diverseKeyboardKey(key), 1);
                    data.EventCallback(event);
                    break;
                }
                } });

        glfwSetMouseButtonCallback(m_Handle, [](GLFWwindow* window, int button, int action, int mods)
                                   {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));

                switch(action)
                {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(GLFWKeyCodes::glfw2diverseMouseKey(button));
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(GLFWKeyCodes::glfw2diverseMouseKey(button));
                    data.EventCallback(event);
                    break;
                }
                } });

        glfwSetScrollCallback(m_Handle, [](GLFWwindow* window, double xOffset, double yOffset)
                              {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));
                MouseScrolledEvent event((float)xOffset, (float)yOffset);
                data.EventCallback(event); });

        glfwSetCursorPosCallback(m_Handle, [](GLFWwindow* window, double xPos, double yPos)
                                 {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));
                MouseMovedEvent event((float)xPos /* * data.DPIScale*/, (float)yPos /* * data.DPIScale*/);
                data.EventCallback(event); });

        glfwSetCursorEnterCallback(m_Handle, [](GLFWwindow* window, int enter)
                                   {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));

                MouseEnterEvent event(enter > 0);
                data.EventCallback(event); });

        glfwSetCharCallback(m_Handle, [](GLFWwindow* window, unsigned int keycode)
                            {
                WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));

                KeyTypedEvent event(GLFWKeyCodes::glfw2diverseKeyboardKey(keycode), char(keycode));
                data.EventCallback(event); });

        glfwSetDropCallback(m_Handle, [](GLFWwindow* window, int numDropped, const char** filenames)
                            {
            WindowData& data = *static_cast<WindowData*>((glfwGetWindowUserPointer(window)));
            WindowFileEvent event(numDropped,filenames);
            data.EventCallback(event); });

        g_MouseCursors[ImGuiMouseCursor_Arrow]      = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_TextInput]  = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_ResizeAll]  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_ResizeNS]   = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_ResizeEW]   = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        g_MouseCursors[ImGuiMouseCursor_Hand]       = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

        DS_LOG_INFO("Initialised GLFW version : {0}", glfwGetVersionString());
        return true;
    }

    std::array<u32,2> GLFWWindow::get_frame_buffer_size() const
    {
        int w, h;
        glfwGetFramebufferSize(m_Handle, &w, &h);
        return std::array<u32,2>{(u32)w,(u32)h};
    }

    void GLFWWindow::set_icon(const WindowDesc& desc)
    {
        std::vector<GLFWimage> images;

        if(!desc.IconData.empty() && desc.IconData[0].second != nullptr)
        {
            for(auto& data : desc.IconData)
            {
                GLFWimage image;

                uint8_t* pixels = nullptr;

                image.height = data.first;
                image.width  = data.first;
                image.pixels = static_cast<unsigned char*>(data.second);
                images.push_back(image);
            }

            glfwSetWindowIcon(m_Handle, int(images.size()), images.data());
        }
        else
        {
            for(auto& path : desc.IconPaths)
            {
                uint32_t width, height;
                GLFWimage image;

                uint8_t* pixels = nullptr;

                if(path != "")
                {
                    pixels = diverse::load_image_from_file(path, &width, &height, nullptr, nullptr, false);

                    if(!pixels)
                    {
                        DS_LOG_WARN("Failed to load app icon {0}", path);
                    }

                    image.height = height;
                    image.width  = width;
                    image.pixels = static_cast<unsigned char*>(pixels);
                    images.push_back(image);
                }
            }

            glfwSetWindowIcon(m_Handle, int(images.size()), images.data());

            for(int i = 0; i < (int)images.size(); i++)
            {
                delete[] images[i].pixels;
            }
        }
    }

    void GLFWWindow::set_window_title(const std::string& title)
    {
        DS_PROFILE_FUNCTION();
        glfwSetWindowTitle(m_Handle, title.c_str());
    }

    void GLFWWindow::toggle_vsync()
    {
        DS_PROFILE_FUNCTION();
        if(m_VSync)
        {
            set_vsync(false);
        }
        else
        {
            set_vsync(true);
        }

        DS_LOG_INFO("VSync : {0}", m_VSync ? "True" : "False");
    }

    void GLFWWindow::set_vsync(bool set)
    {
        DS_PROFILE_FUNCTION();

        m_VSync = set;
        //if(Graphics::GraphicsContext::GetRenderAPI() == RenderAPI::OPENGL)
        //    glfwSwapInterval(set ? 1 : 0);

        DS_LOG_INFO("VSync : {0}", m_VSync ? "True" : "False");
    }

    void GLFWWindow::on_update()
    {
        DS_PROFILE_FUNCTION();
#ifdef DS_RENDER_API_OPENGL
        if(m_Data.m_RenderAPI == RenderAPI::OPENGL)
        {
            DS_PROFILE_SCOPE("GLFW SwapBuffers");
            glfwSwapBuffers(m_Handle);
        }
#endif
    }

    void GLFWWindow::set_borderless_window(bool borderless)
    {
        DS_PROFILE_FUNCTION();
        if(borderless)
        {
            glfwWindowHint(GLFW_DECORATED, false);
        }
        else
        {
            glfwWindowHint(GLFW_DECORATED, true);
        }
    }

    void GLFWWindow::hide_mouse(bool hide)
    {
        DS_PROFILE_FUNCTION();
        if(hide)
        {
            glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else
        {
            glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    void GLFWWindow::set_mouse_position(const glm::vec2& pos)
    {
        DS_PROFILE_FUNCTION();
        Input::get().store_mouse_position(pos.x, pos.y);
        glfwSetCursorPos(m_Handle, pos.x, pos.y);
    }

    void GLFWWindow::MakeDefault()
    {
        CreateFunc = CreateFuncGLFW;
    }

    Window* GLFWWindow::CreateFuncGLFW(const WindowDesc& properties)
    {
        return new GLFWWindow(properties);
    }

    void GLFWWindow::update_cursor_imgui()
    {
        DS_PROFILE_FUNCTION();
        ImGuiIO& io                   = ImGui::GetIO();
        ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();

        if((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(m_Handle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
            return;

        if(imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
        {
            // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor

            // TODO: This was disabled as it would override control of hiding the cursor
            //       Need to find a solution to support both
            // glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        }
        else
        {
            glfwSetCursor(m_Handle, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
            // glfwSetInputMode(m_Handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // #define LOG_CONTROLLER 0

    void GLFWWindow::process_input()
    {
        DS_PROFILE_SCOPE("GLFW PollEvents");
        glfwPollEvents();

        auto& controllers = Input::get().get_controllers();
        for(auto it = controllers.begin(); it != controllers.end();)
        {
            int id = it->first;
            if(glfwJoystickPresent(id) != GLFW_TRUE)
                Input::get().remove_controller(id);

            it++;
        }

        // Update controllers
        for(int id = GLFW_JOYSTICK_1; id < GLFW_JOYSTICK_LAST; id++)
        {
            if(glfwJoystickPresent(id) == GLFW_TRUE)
            {
#if LOG_CONTROLLER
                DS_LOG_INFO("Controller Connected {0}", id);
#endif
                Controller* controller = Input::get().get_or_add_controller(id);
                if(controller)
                {
                    controller->ID   = id;
                    controller->Name = glfwGetJoystickName(id);

#if LOG_CONTROLLER
                    DS_LOG_INFO(controller->Name);
#endif
                    int buttonCount;
                    const unsigned char* buttons = glfwGetJoystickButtons(id, &buttonCount);
                    for(int i = 0; i < buttonCount; i++)
                    {
                        controller->ButtonStates[i] = buttons[i] == GLFW_PRESS;

#if LOG_CONTROLLER
                        if(controller->ButtonStates[i])
                            DS_LOG_INFO("Button pressed {0}", buttons[i]);
#endif
                    }

                    int axisCount;
                    const float* axes = glfwGetJoystickAxes(id, &axisCount);
                    for(int i = 0; i < axisCount; i++)
                    {
                        controller->AxisStates[i] = axes[i];
#if LOG_CONTROLLER
                        DS_LOG_INFO(controller->AxisStates[i]);
#endif
                    }

                    int hatCount;
                    const unsigned char* hats = glfwGetJoystickHats(id, &hatCount);
                    for(int i = 0; i < hatCount; i++)
                    {
                        controller->HatStates[i] = hats[i];
#if LOG_CONTROLLER
                        DS_LOG_INFO(controller->HatStates[i]);
#endif
                    }
                }
            }
        }
    }

    void GLFWWindow::maximise()
    {
        DS_PROFILE_FUNCTION();
        glfwMaximizeWindow(m_Handle);

#ifdef DS_PLATFORM_MACOS
        // TODO: Move to glfw extensions or something
        OS::instance()->maximiseWindow();
#endif
    }
}
