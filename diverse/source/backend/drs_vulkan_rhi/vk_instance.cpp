#include "core/ds_log.h"
#include "vk_instance.h"
#include "maths/maths_utils.h"
#include "core/string.h"
#include "core/version.h"
#include "utility/string_utils.h"
#include <vector>
#include <sstream>
namespace diverse
{
    namespace rhi
    {

		bool checkExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& available_extensions)
		{
			for (const auto& x : available_extensions)
			{
				if (strcmp(x.extensionName, checkExtension) == 0)
				{
					return true;
				}
			}
			return false;
		}

		// Validation layer helpers:
		const std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};
		bool check_validation_layerSupport()
		{
			uint32_t layerCount;
			VkResult res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
			assert(res == VK_SUCCESS);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			res = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
			assert(res == VK_SUCCESS);

			for (const char* layerName : validationLayers) {
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					return false;
				}
			}

			return true;
		}

		bool validate_layers(const std::vector<const char*>& required,
			const std::vector<VkLayerProperties>& available)
		{
			for (auto layer : required)
			{
				bool found = false;
				for (auto& available_layer : available)
				{
					if (strcmp(available_layer.layerName, layer) == 0)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					return false;
				}
			}

			return true;
		}

		std::vector<const char*> get_optimal_validation_layers(const std::vector<VkLayerProperties>& supported_instance_layers)
		{
			std::vector<std::vector<const char*>> validationLayerPriorityList =
			{
				// The preferred validation layer is "VK_LAYER_KHRONOS_validation"
				{"VK_LAYER_KHRONOS_validation"},

				// Otherwise we fallback to using the LunarG meta layer
				{"VK_LAYER_LUNARG_standard_validation"},

				// Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
				{
					"VK_LAYER_GOOGLE_threading",
					"VK_LAYER_LUNARG_parameter_validation",
					"VK_LAYER_LUNARG_object_tracker",
					"VK_LAYER_LUNARG_core_validation",
					"VK_LAYER_GOOGLE_unique_objects",
				},

				// Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
				{"VK_LAYER_LUNARG_core_validation"}
			};

			for (auto& validationLayers : validationLayerPriorityList)
			{
				if (validate_layers(validationLayers, supported_instance_layers))
				{
					return validationLayers;
				}
			}

			// Else return nothing
			return {};
		}

		VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
			VkDebugUtilsMessageTypeFlagsEXT message_type,
			const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
			void* user_data)
		{
			// Log debug messge
			std::string ss;

			if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				ss += "[Vulkan Warning]: ";
				ss += callback_data->pMessage;
				DS_LOG_WARN("{}", ss);
			}
			else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				ss += "[Vulkan Error]: ";
				ss += callback_data->pMessage;
				DS_LOG_WARN("{}", ss);
			}

			return VK_FALSE;
		}

        GpuInstanceVulkan::GpuInstanceVulkan(const DeviceBuilder& builder)
        {
            VkResult res;

            res = volkInitialize();
            assert(res == VK_SUCCESS);
			if( res != VK_SUCCESS)
			{
				DS_LOG_ERROR("Failed to initialize volk");
				exit(-1);
			}
            // Fill out application info:
         	VkApplicationInfo appInfo = {};

            uint32_t sdkVersion    = VK_HEADER_VERSION_COMPLETE;
            uint32_t driverVersion = 0;

            // if enumerateInstanceVersion  is missing, only vulkan 1.0 supported
            // https://www.lunarg.com/wp-content/uploads/2019/02/Vulkan-1.1-Compatibility-Statement_01_19.pdf
            auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

            if(enumerateInstanceVersion)
            {
                enumerateInstanceVersion(&driverVersion);
            }
            else
            {
                driverVersion = VK_API_VERSION_1_0;
            }

            // Choose supported version
            appInfo.apiVersion = maths::Min(sdkVersion, driverVersion);

            // SDK not supported
            if(sdkVersion > driverVersion)
            {
                // Detect and log version
                std::string driverVersionStr = stringutility::to_string(VK_API_VERSION_MAJOR(driverVersion)) + "." + stringutility::to_string(VK_API_VERSION_MINOR(driverVersion)) + "." + stringutility::to_string(VK_API_VERSION_PATCH(driverVersion));
                std::string sdkVersionStr    = stringutility::to_string(VK_API_VERSION_MAJOR(sdkVersion)) + "." + stringutility::to_string(VK_API_VERSION_MINOR(sdkVersion)) + "." + stringutility::to_string(VK_API_VERSION_PATCH(sdkVersion));
#ifndef DS_PRODUCTION
                DS_LOG_WARN("Using Vulkan {0}. Please update your graphics drivers to support Vulkan {1}.", driverVersionStr, sdkVersionStr);
#endif
            }

            appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName   = "divshot";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
            appInfo.pEngineName        = "Diverse";
            appInfo.engineVersion      = VK_MAKE_VERSION(DiverseVersion.major, DiverseVersion.minor, DiverseVersion.patch);
            // Enumerate available layers and extensions:
            uint32_t instanceLayerCount;
            res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
            assert(res == VK_SUCCESS);
            std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
            res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data());
            assert(res == VK_SUCCESS);

            uint32_t extensionCount = 0;
            res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            assert(res == VK_SUCCESS);
            std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
            res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data());
            assert(res == VK_SUCCESS);

            bool debugUtils = false;
            std::vector<const char*> instanceLayers;
            std::vector<const char*> instanceExtensions;

            for (auto& availableExtension : availableInstanceExtensions)
            {
                if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                {
                    debugUtils = true;
                    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
                }
                else if (strcmp(availableExtension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
                {
                    instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
                }
                else if( strcmp(availableExtension.extensionName, "VK_KHR_portability_enumeration") == 0)
                {
                    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                }
                else if (strcmp(availableExtension.extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0)
                {
                    instanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
                }
            }
		    instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
            instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
            instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
            instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
            instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
            instanceExtensions.push_back("VK_EXT_metal_surface");
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
            instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
            instanceExtensions.push_back("VK_EXT_metal_surface");
            instanceExtensions.push_back("VK_EXT_layer_settings");
#endif
            if (builder.graphics_debugging)
            {
                // Determine the optimal validation layers to enable that are necessary for useful debugging
                std::vector<const char*> optimalValidationLyers = get_optimal_validation_layers(availableInstanceLayers);
                instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
            }

            // Create instance:
            // VkInstance	instance;
            VkDebugUtilsMessengerEXT debugUtilsMessenger;
            {
                VkInstanceCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                createInfo.pApplicationInfo = &appInfo;
                createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
                createInfo.ppEnabledLayerNames = instanceLayers.data();
                createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
                createInfo.ppEnabledExtensionNames = instanceExtensions.data();
                createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                
                VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

                if (builder.graphics_debugging && debugUtils)
                {
//                    VkValidationFeatureEnableEXT enables[] = {VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
//                    VkValidationFeaturesEXT features = {};
//                    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
//                    features.enabledValidationFeatureCount = 1;
//                    features.pEnabledValidationFeatures = enables;
//                    debugUtilsCreateInfo.pNext = & features;
                    
                    debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
                    debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                    debugUtilsCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
                    createInfo.pNext = &debugUtilsCreateInfo;
                    
                }
                
                res = vkCreateInstance(&createInfo, nullptr, &instance);
                assert(res == VK_SUCCESS);
				if( res != VK_SUCCESS)
				{
					DS_LOG_ERROR("create vk instance failed!\n");
					exit(-1);
				}
                volkLoadInstanceOnly(instance);

                if (builder.graphics_debugging && debugUtils)
                {
                    res = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &debugUtilsMessenger);
                    assert(res == VK_SUCCESS);
                }
            }
        }
		auto GpuInstanceVulkan::create(DeviceBuilder& builder) -> GpuInstanceVulkan
		{

			VkResult res;

			res = volkInitialize();
			assert(res == VK_SUCCESS);

			// Fill out application info:
			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "fermat";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
			appInfo.pEngineName = "Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
			appInfo.apiVersion = VK_API_VERSION_1_2;
			// Enumerate available layers and extensions:
			uint32_t instanceLayerCount;
			res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
			assert(res == VK_SUCCESS);
			std::vector<VkLayerProperties> availableInstanceLayers(instanceLayerCount);
			res = vkEnumerateInstanceLayerProperties(&instanceLayerCount, availableInstanceLayers.data());
			assert(res == VK_SUCCESS);

			uint32_t extensionCount = 0;
			res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			assert(res == VK_SUCCESS);
			std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
			res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data());
			assert(res == VK_SUCCESS);

			bool debugUtils = false;
			std::vector<const char*> instanceLayers;
			std::vector<const char*> instanceExtensions;

			for (auto& availableExtension : availableInstanceExtensions)
			{
				if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
				{
					debugUtils = true;
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
				}
				else if (strcmp(availableExtension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
				{
					instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
				}
                else if( strcmp(availableExtension.extensionName, "VK_KHR_portability_enumeration") == 0)
                {
                    instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                }
				else if (strcmp(availableExtension.extensionName, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0)
				{
					instanceExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
				}
			}
            
          instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
          instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
          instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
          instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
          instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
          instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
          instanceExtensions.push_back("VK_EXT_metal_surface");
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
          instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
          instanceExtensions.push_back("VK_EXT_metal_surface");
#endif
			if (builder.graphics_debugging)
			{
				// Determine the optimal validation layers to enable that are necessary for useful debugging
				std::vector<const char*> optimalValidationLyers = get_optimal_validation_layers(availableInstanceLayers);
				instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
			}

			// Create instance:
			VkInstance	instance;
			VkDebugUtilsMessengerEXT debugUtilsMessenger;
			{
				VkInstanceCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
				createInfo.pApplicationInfo = &appInfo;
				createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
				createInfo.ppEnabledLayerNames = instanceLayers.data();
				createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
				createInfo.ppEnabledExtensionNames = instanceExtensions.data();

				VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

				if (builder.graphics_debugging && debugUtils)
				{
					debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
					debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
					debugUtilsCreateInfo.pfnUserCallback = debugUtilsMessengerCallback;
					createInfo.pNext = &debugUtilsCreateInfo;
				}

				res = vkCreateInstance(&createInfo, nullptr, &instance);
				assert(res == VK_SUCCESS);
				if(res != VK_SUCCESS)
				{
					DS_LOG_ERROR("create vkInstance Failed");
				}
				volkLoadInstanceOnly(instance);

				if (builder.graphics_debugging && debugUtils)
				{
					res = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, nullptr, &debugUtilsMessenger);
					assert(res == VK_SUCCESS);
				}
			}
			return { instance };
		}
    }
}
