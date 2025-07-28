#if defined(DS_RENDER_API_VULKAN) && !defined(DS_PLATFORM_MACOS) && !defined(DS_PLATFORM_IOS)

// #include "platform/Vulkan/VKSwapChain.h"
#include "engine/application.h"
#include "engine/window.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace diverse
{
    // VkSurfaceKHR Graphics::VKSwapChain::CreatePlatformSurface(VkInstance vkInstance, Window* window)
    // {
    //     VkSurfaceKHR surface;

    //     glfwCreateWindowSurface(vkInstance, static_cast<GLFWwindow*>(window->GetHandle()), nullptr, (VkSurfaceKHR*)&surface);

    //     return surface;
    // }

    static const char* GetPlatformSurfaceExtension()
    {
        return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
    }
}

#endif
