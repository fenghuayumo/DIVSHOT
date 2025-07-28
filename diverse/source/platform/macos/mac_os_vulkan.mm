#ifdef DS_RENDER_API_VULKAN

#import <Cocoa/Cocoa.h>
#include <QuartzCore/CAMetalLayer.h>

#include "engine/window.h"
#include "engine/application.h"
#undef _GLFW_REQUIRE_LOADER
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dlfcn.h>
#include "backend/drs_vulkan_rhi/vk_utils.h"
#include <MoltenVK/vk_mvk_moltenvk.h>

extern "C" void* GetCAMetalLayer(void* handle)
{
    NSWindow* window = (NSWindow*)handle;
    NSView* view = window.contentView;
    
    if (![view.layer isKindOfClass:[CAMetalLayer class]])
    {
        [view setLayer:[CAMetalLayer layer]];
        [view setWantsLayer:YES];
        [view.layer setContentsScale:[window backingScaleFactor]];
    }
    
    return view.layer;
}

namespace diverse
{
	VkSurfaceKHR create_platform_surface(VkInstance vkInstance, Window* window)
	{
        VkSurfaceKHR surface;
        // NSWindow *nswin = glfwGetCocoaWindow(static_cast<GLFWwindow*>(window->get_handle()));
        // auto nsView = [nswin contentView];        // 获取 NSView
        // [nsView setCanDrawConcurrently:YES];
        // [nswin setAllowsConcurrentViewDrawing:YES];
        // [NSProcessInfo.processInfo disableAutomaticTermination:@"Rendering in background"];
#if defined(VK_USE_PLATFORM_METAL_EXT)
		
        VkMetalSurfaceCreateInfoEXT surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        surfaceInfo.pNext = NULL;
        surfaceInfo.flags = 0;
        surfaceInfo.pLayer = (CAMetalLayer*)GetCAMetalLayer((void*)glfwGetCocoaWindow(static_cast<GLFWwindow*>(window->get_handle())));
        VK_CHECK_RESULT(vkCreateMetalSurfaceEXT(vkInstance, &surfaceInfo, nullptr, &surface));
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
        VkMacOSSurfaceCreateInfoMVK surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        surfaceInfo.pNext = NULL;
        surfaceInfo.flags = 0;
        surfaceInfo.pView = GetCAMetalLayer((void*)glfwGetCocoaWindow(static_cast<GLFWwindow*>(window->GetHandle())));
        vkCreateMacOSSurfaceMVK(vkInstance, &surfaceInfo, nullptr, &surface);
#endif
		
//		 auto libMoltenVK = dlopen("/usr/local/lib/libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
//		 auto getMoltenVKConfigurationMVK = (PFN_vkGetMoltenVKConfigurationMVK)
//		 	dlsym(libMoltenVK, "vkGetMoltenVKConfigurationMVK");
//		 auto setMoltenVKConfigurationMVK = (PFN_vkSetMoltenVKConfigurationMVK)
//		 	dlsym(libMoltenVK, "vkSetMoltenVKConfigurationMVK");
//		
//		 MVKConfiguration mvkConfig;
//        size_t pConfigurationSize = sizeof(MVKConfiguration);
//        getMoltenVKConfigurationMVK(vkInstance, &mvkConfig, &pConfigurationSize);
//		 #ifndef DS_PLATFORM_PRODUCTION
//        mvkConfig.debugMode = true;
//		 #endif
////        mvkConfig.synchronousQueueSubmits = true;
//       //  mvkConfig.presentWithCommandBuffer = false;
////         mvkConfig.prefillMetalCommandBuffers = MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS_STYLE_NO_PREFILL;//MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS_STYLE_DEFERRED_ENCODING;
//
//        mvkConfig.useMetalArgumentBuffers = MVKUseMetalArgumentBuffers::MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS_ALWAYS;
//        mvkConfig.resumeLostDevice = true;
//		
//        setMoltenVKConfigurationMVK(vkInstance, &mvkConfig, &pConfigurationSize);
		
		return surface;
	}
}

#endif
