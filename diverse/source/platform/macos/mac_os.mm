#include "mac_os.h"
#include "mac_os_power.h"
#include "platform/glfw/glfw_window.h"
#include "engine/core_system.h"
#include "engine/application.h"
#include "core/memory_manager.h"

#include <mach-o/dyld.h>

#import <Cocoa/Cocoa.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

extern diverse::Application* diverse::createApplication();

namespace diverse
{
    void MacOSOS::run()
    {
        auto power = MacOSPower();
        auto percentage = power.GetPowerPercentageLeft();
        auto secondsLeft = power.GetPowerSecondsLeft();
        auto state = power.GetPowerState();
		
		int hours, minutes;
		minutes = secondsLeft / 60;
		hours = minutes / 60;
		minutes = minutes % 60;
		
        DS_LOG_INFO("--------------------");
        DS_LOG_INFO(" System Information ");
        DS_LOG_INFO("--------------------");

        if(state != PowerState::POWERSTATE_NO_BATTERY)
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

    void MacOSOS::init()
    {
        GLFWWindow::MakeDefault();
    }

    void MacOSOS::setTitleBarColour(const glm::vec4& colour, bool dark)
    {
        auto& app = diverse::Application::get();

        NSWindow* window = (NSWindow*)glfwGetCocoaWindow(static_cast<GLFWwindow*>(app.get_window()->get_handle()));
        window.titlebarAppearsTransparent = YES;
        //window.titleVisibility = NSWindowTitleHidden;
        
        NSColor *titleColour = [NSColor colorWithSRGBRed:colour.x green:colour.y blue:colour.z alpha:colour.w];
        window.backgroundColor = titleColour;
        if(dark)
            window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameVibrantDark];
        else
            window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameVibrantLight];
    }

    std::string MacOSOS::getExecutablePath()
    {
        std::string result;

        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::vector<char> buffer;
        buffer.resize(size + 1);

        _NSGetExecutablePath(buffer.data(), &size);
        buffer[size] = '\0';

        if (!strrchr(buffer.data(), '/'))
        {
            return "";
        }
        return buffer.data();
    }
	
	void MacOSOS::delay(uint32_t usec)
	{
		struct timespec requested = { static_cast<time_t>(usec / 1000000), (static_cast<long>(usec) % 1000000) * 1000 };
		struct timespec remaining;
		while (nanosleep(&requested, &remaining) == -1)
		{
			requested.tv_sec = remaining.tv_sec;
			requested.tv_nsec = remaining.tv_nsec;
		}
	}

    void MacOSOS::maximiseWindow()
    {
        auto window = Application::get().get_window();
        NSWindow* nativeWindow = glfwGetCocoaWindow((GLFWwindow*)window->get_handle());

        [nativeWindow toggleFullScreen:nil];
    }
}
