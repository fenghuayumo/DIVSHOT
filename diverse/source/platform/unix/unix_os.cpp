#include "precompile.h"
#include "unix_os.h"
#include "platform/glfw/glfw_window.h"
#include "engine/core_system.h"
#include "core/memory_manager.h"
#include "engine/application.h"

#include <time.h>

extern diverse::Application* diverse::createApplication();

namespace diverse
{
    void UnixOS::run()
    {
        auto& app = diverse::Application::get();

        DS_LOG_INFO("--------------------");
        DS_LOG_INFO(" System Information ");
        DS_LOG_INFO("--------------------");

        auto systemInfo = MemoryManager::get()->get_system_info();
        systemInfo.log();

        app.init();
        app.run();
        app.release();
    }

    void UnixOS::init()
    {
        GLFWWindow::MakeDefault();
    }

    SystemMemoryInfo MemoryManager::get_system_info()
    {
        SystemMemoryInfo result = {};
        return result;
    }

    void UnixOS::delay(uint32_t usec)
    {
        struct timespec requested = { static_cast<time_t>(usec / 1000000), (static_cast<long>(usec) % 1000000) * 1000 };
        struct timespec remaining;
        while(nanosleep(&requested, &remaining) == -1)
        {
            requested.tv_sec  = remaining.tv_sec;
            requested.tv_nsec = remaining.tv_nsec;
        }
    }

    void UnixOS::openFileLocation(const std::filesystem::path& path)
    {
#ifndef diverse_PLATFORM_MOBILE
        std::string command = "open -R " + path.string();
        std::system(command.c_str());
#endif
    }

    void UnixOS::openFileExternal(const std::filesystem::path& path)
    {
#ifndef diverse_PLATFORM_MOBILE
        std::string command = "open " + path.string();
        std::system(command.c_str());
#endif
    }

    void UnixOS::openURL(const std::string& url)
    {
#ifndef diverse_PLATFORM_MOBILE
        std::string command = "open " + url;
        system(command.c_str());
#endif
    }
}
