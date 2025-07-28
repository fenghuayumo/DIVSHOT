#ifdef DS_PLATFORM_WINDOWS
#include <string>
#include "backend/drs_vulkan_rhi/vk_utils.h"
#if DS_USE_GLFW_WINDOWS
#include "platform/glfw/glfw_window.h"
#include <GLFW/glfw3.h>
#include "platform/windows/windows_window.h"
#endif

namespace diverse
{
    VkSurfaceKHR    create_platform_surface(VkInstance vkInstance, Window* window)
    {
        VkSurfaceKHR surface;
    #if DS_USE_GLFW_WINDOWS
        {   
            auto fw_window = static_cast<GLFWWindow*>(window)->get_handle();
            VK_CHECK_RESULT(glfwCreateWindowSurface(vkInstance, reinterpret_cast<GLFWwindow*>(fw_window),nullptr, &surface));
        }
    #else

        VkWin32SurfaceCreateInfoKHR surfaceInfo;
        memset(&surfaceInfo, 0, sizeof(VkWin32SurfaceCreateInfoKHR));

        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.pNext = NULL;
        surfaceInfo.hwnd = static_cast<HWND>(reinterpret_cast<WindowsWindow*>(window)->GetHandle());
        surfaceInfo.hinstance = dynamic_cast<WindowsWindow*>(window)->GetHInstance();
        vkCreateWin32SurfaceKHR(vkInstance, &surfaceInfo, nullptr, &surface);

    #endif
        return surface;
    }
}
#endif