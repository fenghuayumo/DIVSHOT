#include "windows_os.h"
#include "windows_power.h"
#include "windows_window.h"
#include "engine/core_system.h"
#include "core/memory_manager.h"
#include "engine/application.h"
#include <Windows.h>

#ifdef DS_USE_GLFW_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32

#include "platform/glfw/glfw_window.h"
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#endif

#include <filesystem>

#include <shellapi.h>
#include <dwmapi.h>
#include <winuser.h>

extern diverse::Application* diverse::createApplication();

namespace diverse
{
    void WindowsOS::run()
    {
        auto power = WindowsPower();
        auto percentage = power.get_power_percentage_left();
        auto secondsLeft = power.get_power_seconds_left();
        auto state = power.ge_power_state();

        DS_LOG_INFO("--------------------");
        DS_LOG_INFO(" System Information ");
        DS_LOG_INFO("--------------------");

        if (state != PowerState::POWERSTATE_NO_BATTERY)
            DS_LOG_INFO("Battery Info - Percentage : {0} , Time Left {1}s , State : {2}", percentage, secondsLeft, powerStateToString(state));
        else
            DS_LOG_INFO("Power - Outlet");

        auto systemInfo = MemoryManager::get()->get_system_info();
        systemInfo.log();

        auto& app = diverse::Application::get();
        app.init();
        app.run();
        app.release();
    }

    void WindowsOS::init()
    {
#ifdef DS_USE_GLFW_WINDOWS
        GLFWWindow::MakeDefault();
#else
        WindowsWindow::MakeDefault();
#endif
    }

    SystemMemoryInfo MemoryManager::get_system_info()
    {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&status);

        SystemMemoryInfo result = {
            (int64_t)status.ullAvailPhys,
            (int64_t)status.ullTotalPhys,

            (int64_t)status.ullAvailVirtual,
            (int64_t)status.ullTotalVirtual
        };
        return result;
    }

    std::string WindowsOS::getExecutablePath()
    {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);

        std::string convertedString = std::filesystem::path(path).string();
        std::replace(convertedString.begin(), convertedString.end(), '\\', '/');

        return convertedString;
    }

    void WindowsOS::openFileLocation(const std::filesystem::path& path)
    {
        ShellExecuteA(NULL, "open", std::filesystem::is_directory(path) ? path.string().c_str() : path.parent_path().string().c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    void WindowsOS::openFileExternal(const std::filesystem::path& path)
    {
        ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    void WindowsOS::openURL(const std::string& url)
    {
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

#include <glfw/glfw3native.h>

    void WindowsOS::setTitleBarColour(const glm::vec4& colour, bool dark)
    {
#if WINVER >= 0x0A00
        auto& app = diverse::Application::get();
        HWND hwnd = glfwGetWin32Window((GLFWwindow*)static_cast<GLFWwindow*>(app.get_window()->get_handle()));

        COLORREF col = RGB(colour.x * 255, colour.y * 255, colour.z * 255);

        COLORREF CAPTION_COLOR = col;
        COLORREF BORDER_COLOR = 0x201e1e;

        DwmSetWindowAttribute(hwnd, 34 /*DWMWINDOWATTRIBUTE::DWMWA_BORDER_COLOR*/, &BORDER_COLOR, sizeof(BORDER_COLOR));
        DwmSetWindowAttribute(hwnd, 35 /*DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR*/, &CAPTION_COLOR, sizeof(CAPTION_COLOR));
        SetWindowPos(hwnd, NULL, NULL, NULL, NULL, NULL, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE);
#endif
    }
}
