#include "gpu_device_vulkan.h"
#include "gpu_texture_vulkan.h"
#include "gpu_buffer_vulkan.h"
#include "gpu_barrier_vulkan.h"
#include "gpu_pipeline_vulkan.h"
#include "gpu_raytracing_vulkan.h"
#include "vk_barrier_utils.h"
#ifdef USE_VMA_ALLOCATOR
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vulkan/vk_mem_alloc.h>
#endif

#include <ranges>
#include "core/ds_log.h"
#define SMALL_ALLOCATION_MAX_SIZE 4096

namespace diverse
{
    namespace rhi
    {
        constexpr u32 MAX_SET_COUNT = 4;
        GpuCommandBufferVulkan::GpuCommandBufferVulkan(VkDevice device, Queue	que)
            : family_index(que.family.index), queue(que.queue)
        {
            VkCommandPoolCreateInfo pool_create_info = {};
            pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            pool_create_info.queueFamilyIndex = family_index;
            pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

            VK_CHECK_RESULT(vkCreateCommandPool(device, &pool_create_info, nullptr, &pool));
            VkCommandBufferAllocateInfo	command_buffer_allocate_info = {};
            command_buffer_allocate_info.commandBufferCount = 1;
            command_buffer_allocate_info.commandPool = pool;
            command_buffer_allocate_info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

            VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &command_buffer_allocate_info, &handle));

            VkFenceCreateInfo fence_info = {};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_CHECK_RESULT(vkCreateFence(device, &fence_info, nullptr, &submit_done_fence));
        }

        GpuCommandBufferVulkan::~GpuCommandBufferVulkan()
        {
            if( handle != VK_NULL_HANDLE)
            {
                auto device = dynamic_cast<GpuDeviceVulkan*>(get_global_device());
                vkDestroyFence(device->device, submit_done_fence, nullptr);
                vkDestroyCommandPool(device->device, pool, nullptr);
            }
        }

        extern bool checkExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& available_extensions);

        DeviceFrameVulkan::~DeviceFrameVulkan()
        {
        }

        GpuDeviceVulkan::GpuDeviceVulkan(u32 device_index)
        {
            DeviceBuilder	builder = {};
#ifdef DS_PRODUCTION
            builder.graphics_debugging = false;
#endif
            instance = std::make_shared<GpuInstanceVulkan>(builder);

            auto physical_devices = enumerate_physical_devices(*instance);
            //physical_devices = presentation_support(physical_devices, *surface);
            for (auto& dev : physical_devices)
            {
                DS_LOG_INFO("Availiable Device: {}", dev.properties.deviceName);
            }
            PhysicalDevice	pdevice;
            if ((device_index >= 0 && device_index < physical_devices.size())) {
                pdevice = physical_devices[device_index];
            }
            else {
                std::vector<int>    scores(physical_devices.size());
                for (int i = 0; i < physical_devices.size(); i++) {
                    //sort according to device type
                    switch (physical_devices[i].properties.deviceType)
                    {
                    case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    {
                        scores[i] = 200;
                    }break;
                    case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    {
                        scores[i] = 1000;
                    }break;
                    case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    {
                        scores[i] = 1;
                    }break;
                    default:
                        scores[i] = 0;
                        break;
                    }
                }
                int max_scores = scores[0];
                for (int i = 0; i < physical_devices.size(); i++) {
                    if( scores[i] >= max_scores)
                        device_index = i;
                }
            }
            if ((device_index >= 0 && device_index < physical_devices.size())) 
                pdevice = physical_devices[device_index];
            DS_LOG_INFO("Selected physical device: {}", device_index);
            physcial_device = pdevice;

            uint32_t extCount = 0;
            auto ext_result = vkEnumerateDeviceExtensionProperties(pdevice.handle, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> supported_extensions(extCount);
            if (extCount > 0)
            {
                vkEnumerateDeviceExtensionProperties(pdevice.handle, nullptr, &extCount, &supported_extensions.front());
            }
            auto device_extension_names = std::vector<const char*>{
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                VK_KHR_MAINTENANCE2_EXTENSION_NAME,
                VK_KHR_MAINTENANCE3_EXTENSION_NAME,
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
                VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
                VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
                VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
                "VK_KHR_synchronization2",
                "VK_KHR_buffer_device_address",
    #if defined(DS_PLATFORM_MACOS)
        "VK_KHR_portability_subset",
//        "VK_KHR_maintenance4",
//        "VK_EXT_descriptor_buffer",
//            "VK_EXT_shader_atomic_float",
//            "VK_EXT_shader_demote_to_helper_invocation",
//            "VK_EXT_shader_stencil_export",
//            "VK_EXT_shader_subgroup_ballot",
//            "VK_EXT_shader_subgroup_vote",
//            "VK_EXT_shader_viewport_index_layer",
//            "VK_EXT_subgroup_size_control",
//            "VK_KHR_shader_draw_parameters",
//            "VK_KHR_shader_float_controls",
//            "VK_KHR_shader_float16_int8",
//            "VK_KHR_shader_integer_dot_product",
//            "VK_KHR_shader_non_semantic_info",
//            "VK_KHR_shader_subgroup_extended_types",
//            "VK_KHR_spirv_1_4",
//            "VK_KHR_storage_buffer_storage_class",
    #endif
    #if DLSS
                "VK_NVX_binary_import",
                "VK_KHR_push_descriptor",
                VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME
    #endif
            };

            auto ray_tracing_extension_names = std::vector<const char*>{
                VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
                VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            };
            for (auto ext : ray_tracing_extension_names)
            {
                if (checkExtensionSupport(ext, supported_extensions))
                    gpu_limits.ray_tracing_enabled = true;
                else
                {
                    DS_LOG_INFO("Ray tracing extension not supported: {}", ext);
                    gpu_limits.ray_tracing_enabled = false;
                }
            }
          
            // Check if the extension is supported
            VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT topologyRestartFeatures = {};
            topologyRestartFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT;
            if (checkExtensionSupport("VK_EXT_primitive_topology_list_restart", supported_extensions)){
                topologyRestartFeatures.primitiveTopologyListRestart = VK_TRUE;
                device_extension_names.push_back("VK_EXT_primitive_topology_list_restart");
            }
            VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
            rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
            if (checkExtensionSupport(VK_KHR_RAY_QUERY_EXTENSION_NAME, supported_extensions)){
                gpu_limits.ray_tracing_enabled = true;
                rayQueryFeatures.rayQuery = VK_TRUE;
                device_extension_names.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            }
            if (gpu_limits.ray_tracing_enabled)
            {
                DS_LOG_INFO("All ray tracing extensions are supported");
                device_extension_names.insert(device_extension_names.end(), ray_tracing_extension_names.begin(), ray_tracing_extension_names.end());
            }
            if (pdevice.presentation_requested)
            {
                device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }
            for (auto ext : device_extension_names)
            {
                if (!checkExtensionSupport(ext, supported_extensions))
                {
                    DS_LOG_ERROR("Device extension not supported: {}", ext);
                    exit(-1);
                }
            }
            float priorities = 1.0;
            auto find_graphics_que_fam = std::find_if(pdevice.queue_family.begin(), pdevice.queue_family.end(), [](QueueFamily que_fam)->bool {
                return que_fam.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
                });
            auto find_compute_que_fam = std::find_if(pdevice.queue_family.begin(), pdevice.queue_family.end(), [find_graphics_que_fam](QueueFamily que_fam)->bool {
                return (que_fam.properties.queueFlags & VK_QUEUE_COMPUTE_BIT) && que_fam.index != find_graphics_que_fam->index;
                });
            auto find_transfer_que_fam = std::find_if(pdevice.queue_family.begin(), pdevice.queue_family.end(), [find_graphics_que_fam, find_compute_que_fam](QueueFamily que_fam)->bool {
                return (que_fam.properties.queueFlags & VK_QUEUE_TRANSFER_BIT)  && que_fam.index != find_graphics_que_fam->index &&
                    que_fam.index != find_compute_que_fam->index;
                });
            QueueFamily universal_queue,compute_queue_fam,transfer_queue_fam;
            if (pdevice.queue_family.begin() <= find_graphics_que_fam && find_graphics_que_fam < pdevice.queue_family.end())
                universal_queue = *find_graphics_que_fam;
            else {
                DS_LOG_ERROR("No suitable render queue found");
            }
            if (pdevice.queue_family.begin() <= find_compute_que_fam && find_compute_que_fam < pdevice.queue_family.end())
                compute_queue_fam = *find_compute_que_fam;
            else {
                DS_LOG_ERROR("No suitable compute queue found");
            }
            if (pdevice.queue_family.begin() <= find_transfer_que_fam && find_transfer_que_fam < pdevice.queue_family.end())
                transfer_queue_fam = *find_transfer_que_fam;
            else {
                DS_LOG_ERROR("No suitable transfer queue found");
            }
//            for (auto queue_fam_index = 0; queue_fam_index < pdevice.queue_family.size(); queue_fam_index++) {
//                auto queue_fam = pdevice.queue_family[queue_fam_index];
//                    VkBool32 bSupported = VK_FALSE;
//                    vkGetPhysicalDeviceSurfaceSupportKHR(pdevice.handle, queue_fam_index, surface, &bSupported);
//                    if (bSupported) {
//                        
//                        break;
//                    }
//                }
//            }
            //auto universal_queue_info =
            VkDeviceQueueCreateInfo	device_queue_create_info[3] = {};
            device_queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_info[0].queueFamilyIndex = universal_queue.index;
            device_queue_create_info[0].pQueuePriorities = &priorities;
            device_queue_create_info[0].queueCount = 1;

            device_queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_info[1].queueFamilyIndex = compute_queue_fam.index;
            device_queue_create_info[1].pQueuePriorities = &priorities;
            device_queue_create_info[1].queueCount = 1;
             
            device_queue_create_info[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_info[2].queueFamilyIndex = transfer_queue_fam.index;
            device_queue_create_info[2].pQueuePriorities = &priorities;
            device_queue_create_info[2].queueCount = 1;

            VkPhysicalDeviceScalarBlockLayoutFeatures	scalar_block = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES };
            VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptor_indexing = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
            VkPhysicalDeviceImagelessFramebufferFeaturesKHR	imageless_framebuffer = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES };
            VkPhysicalDeviceShaderFloat16Int8Features shader_float16_int8 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES };
            VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkan_memory_model = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES };
            VkPhysicalDeviceBufferDeviceAddressFeatures get_buffer_device_address_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
            VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
            VkPhysicalDeviceRayTracingPipelineFeaturesKHR	ray_tracing_pipeline_features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
            get_buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

            VkPhysicalDeviceFeatures2	feature2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
            VkPhysicalDeviceSynchronization2Features synchronization2Features = {};
            synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
            synchronization2Features.synchronization2 = VK_TRUE;
            feature2.pNext = &synchronization2Features;
            synchronization2Features.pNext = &scalar_block;
            scalar_block.pNext = &descriptor_indexing;
            descriptor_indexing.pNext = &imageless_framebuffer;
            imageless_framebuffer.pNext = &shader_float16_int8;
            shader_float16_int8.pNext = &vulkan_memory_model;
            vulkan_memory_model.pNext = &get_buffer_device_address_features;
            
            if (gpu_limits.ray_tracing_enabled)
            {
                get_buffer_device_address_features.pNext = &acceleration_structure_features;
                acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
                ray_tracing_pipeline_features.pNext = &rayQueryFeatures;
                if( topologyRestartFeatures.primitiveTopologyListRestart)
                    rayQueryFeatures.pNext = &topologyRestartFeatures;
            }else if( topologyRestartFeatures.primitiveTopologyListRestart)
                get_buffer_device_address_features.pNext = &topologyRestartFeatures;
            
            vkGetPhysicalDeviceFeatures2(pdevice.handle, &feature2);
            gpu_limits.rayQuery = rayQueryFeatures.rayQuery;
            {
                assert(scalar_block.scalarBlockLayout != 0);

                assert(descriptor_indexing.shaderUniformTexelBufferArrayDynamicIndexing != 0);
                assert(descriptor_indexing.shaderStorageTexelBufferArrayDynamicIndexing != 0);
                assert(descriptor_indexing.shaderSampledImageArrayNonUniformIndexing != 0);
                assert(descriptor_indexing.shaderStorageImageArrayNonUniformIndexing != 0);
                assert(descriptor_indexing.shaderUniformTexelBufferArrayNonUniformIndexing != 0);
                assert(descriptor_indexing.shaderStorageTexelBufferArrayNonUniformIndexing != 0);
                assert(descriptor_indexing.descriptorBindingSampledImageUpdateAfterBind != 0);
                assert(descriptor_indexing.descriptorBindingUpdateUnusedWhilePending != 0);
                assert(descriptor_indexing.descriptorBindingPartiallyBound != 0);
                assert(descriptor_indexing.descriptorBindingVariableDescriptorCount != 0);
                assert(descriptor_indexing.runtimeDescriptorArray != 0);

                assert(imageless_framebuffer.imagelessFramebuffer != 0);

                assert(shader_float16_int8.shaderInt8 != 0);

                if (gpu_limits.ray_tracing_enabled)
                {
                    assert(descriptor_indexing.shaderUniformBufferArrayNonUniformIndexing != 0);
                    assert(descriptor_indexing.shaderStorageBufferArrayNonUniformIndexing != 0);

                    assert(vulkan_memory_model.vulkanMemoryModel != 0);

                    assert(acceleration_structure_features.accelerationStructure != 0);
                    assert(acceleration_structure_features.descriptorBindingAccelerationStructureUpdateAfterBind != 0);

                    assert(ray_tracing_pipeline_features.rayTracingPipeline != 0);
                    assert(ray_tracing_pipeline_features.rayTracingPipelineTraceRaysIndirect != 0);

                    assert(get_buffer_device_address_features.bufferDeviceAddress != 0);
                }
            }
            
            VkDeviceCreateInfo	device_create_info = {};
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pQueueCreateInfos = device_queue_create_info;
            device_create_info.ppEnabledExtensionNames = device_extension_names.data();
            device_create_info.pNext = &feature2;
            device_create_info.queueCreateInfoCount = 3;
            device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extension_names.size());
            device_create_info.pEnabledFeatures = nullptr;
            device_create_info.enabledLayerCount = 0;
            VK_CHECK_RESULT(vkCreateDevice(pdevice.handle, &device_create_info, nullptr, &device));

            VkPhysicalDeviceMemoryProperties memoryProperties;
            vkGetPhysicalDeviceMemoryProperties(physcial_device.handle, &memoryProperties);

            uint64_t gpuMemorySize = 0;
            for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
                if (memoryProperties.memoryHeaps[i].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                gpuMemorySize = memoryProperties.memoryHeaps[i].size;
                    break;
                }
            }
            gpu_limits.vram_size = gpuMemorySize;

            volkLoadDevice(device);
            VkQueue	vk_universe_queue,vk_compute_queue,vk_transfer_queue;
            vkGetDeviceQueue(device, universal_queue.index, 0, &vk_universe_queue);
            universe_queue = Queue{
                vk_universe_queue,
                universal_queue
            };
            vkGetDeviceQueue(device, compute_queue_fam.index, 0, &vk_compute_queue);
            compute_queue = Queue{
                vk_compute_queue,
                compute_queue_fam
            };
            vkGetDeviceQueue(device, transfer_queue_fam.index, 0, &vk_transfer_queue);
            transfer_queue = Queue{
                vk_transfer_queue,
                transfer_queue_fam
            };
            VmaAllocatorCreateInfo allocatorInfo = {};
            allocatorInfo.physicalDevice = physcial_device.handle;
            allocatorInfo.device = device;
            allocatorInfo.instance = instance->instance;

            VmaVulkanFunctions fn;
            fn.vkAllocateMemory = (PFN_vkAllocateMemory)vkAllocateMemory;
            fn.vkBindBufferMemory = (PFN_vkBindBufferMemory)vkBindBufferMemory;
            fn.vkBindImageMemory = (PFN_vkBindImageMemory)vkBindImageMemory;
            fn.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkCmdCopyBuffer;
            fn.vkCreateBuffer = (PFN_vkCreateBuffer)vkCreateBuffer;
            fn.vkCreateImage = (PFN_vkCreateImage)vkCreateImage;
            fn.vkDestroyBuffer = (PFN_vkDestroyBuffer)vkDestroyBuffer;
            fn.vkDestroyImage = (PFN_vkDestroyImage)vkDestroyImage;
            fn.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkFlushMappedMemoryRanges;
            fn.vkFreeMemory = (PFN_vkFreeMemory)vkFreeMemory;
            fn.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkGetBufferMemoryRequirements;
            fn.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkGetImageMemoryRequirements;
            fn.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetPhysicalDeviceMemoryProperties;
            fn.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetPhysicalDeviceProperties;
            fn.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vkInvalidateMappedMemoryRanges;
            fn.vkMapMemory = (PFN_vkMapMemory)vkMapMemory;
            fn.vkUnmapMemory = (PFN_vkUnmapMemory)vkUnmapMemory;
            fn.vkGetBufferMemoryRequirements2KHR = 0; //(PFN_vkGetBufferMemoryRequirements2KHR)vkGetBufferMemoryRequirements2KHR;
            fn.vkGetImageMemoryRequirements2KHR = 0; //(PFN_vkGetImageMemoryRequirements2KHR)vkGetImageMemoryRequirements2KHR;
            fn.vkBindImageMemory2KHR = 0;
            fn.vkBindBufferMemory2KHR = 0;
            fn.vkGetPhysicalDeviceMemoryProperties2KHR = 0;
            fn.vkGetImageMemoryRequirements2KHR = 0;
            fn.vkGetBufferMemoryRequirements2KHR = 0;
            allocatorInfo.pVulkanFunctions = &fn;
            allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            if (vmaCreateAllocator(&allocatorInfo, &global_allocator) != VK_SUCCESS)
            {
                DS_LOG_INFO("[VULKAN] Failed to create VMA allocator");
            }
                    
            VkPhysicalDeviceProperties2 properties2 = {};
            VkPhysicalDeviceVulkan11Properties properties_1_1 = {};
            VkPhysicalDeviceVulkan12Properties properties_1_2 = {};
            
            VkPhysicalDeviceDescriptorIndexingProperties descriptorIndexingProperties = {};
            descriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
            VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{};
            hostProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;

            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            properties_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;

            properties2.pNext = &hostProperties;
            hostProperties.pNext = &descriptorIndexingProperties;
            descriptorIndexingProperties.pNext = &properties_1_1;
            properties_1_1.pNext = &properties_1_2;
            void** properties_chain = &properties_1_2.pNext;

            asProperties = {};
            raytracingProperties = {};

            if (checkExtensionSupport(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, supported_extensions))
            {
                asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
                *properties_chain = &asProperties;
                properties_chain = &asProperties.pNext;
                if (checkExtensionSupport(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, supported_extensions))
                {
                    raytracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                    *properties_chain = &raytracingProperties;
                    properties_chain = &raytracingProperties.pNext;
                }

                if (checkExtensionSupport(VK_KHR_RAY_QUERY_EXTENSION_NAME, supported_extensions))
                {
                    
                }


#define GetDeviceProcAddr(name) \
	            name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name))

                GetDeviceProcAddr(vkGetBufferDeviceAddressKHR);
                GetDeviceProcAddr(vkCmdBuildAccelerationStructuresKHR);
                GetDeviceProcAddr(vkBuildAccelerationStructuresKHR);
                GetDeviceProcAddr(vkCreateAccelerationStructureKHR);
                GetDeviceProcAddr(vkDestroyAccelerationStructureKHR);
                GetDeviceProcAddr(vkGetAccelerationStructureBuildSizesKHR);
                GetDeviceProcAddr(vkGetAccelerationStructureDeviceAddressKHR);
                GetDeviceProcAddr(vkCmdTraceRaysKHR);
                GetDeviceProcAddr(vkGetRayTracingShaderGroupHandlesKHR);
                GetDeviceProcAddr(vkCreateRayTracingPipelinesKHR);
            }
         
            vkGetPhysicalDeviceProperties2(physcial_device.handle, &properties2);
         
            gpu_limits.max_per_stage_descriptor_sampled_images = physcial_device.properties.limits.maxPerStageDescriptorSampledImages;
            gpu_limits.max_image_dimension1_d = physcial_device.properties.limits.maxImageDimension1D;
            gpu_limits.max_image_dimension2_d = physcial_device.properties.limits.maxImageDimension2D;
            gpu_limits.max_image_dimension3_d = physcial_device.properties.limits.maxImageDimension3D;
            gpu_limits.max_image_dimension_cube = physcial_device.properties.limits.maxImageDimensionCube;
            gpu_limits.minStorageBufferOffsetAlignment = physcial_device.properties.limits.minStorageBufferOffsetAlignment;
            gpu_limits.minTexelBufferOffsetAlignment = physcial_device.properties.limits.minTexelBufferOffsetAlignment;
            gpu_limits.minUniformBufferOffsetAlignment = physcial_device.properties.limits.minUniformBufferOffsetAlignment;
            gpu_limits.max_per_stage_descriptor_storage_images = physcial_device.properties.limits.maxPerStageDescriptorStorageImages;
            gpu_limits.max_per_stage_descriptor_storage_buffers = physcial_device.properties.limits.maxPerStageDescriptorStorageBuffers;
            gpu_limits.max_per_stage_descriptor_unifrom_texel_buffers = physcial_device.properties.limits.maxPerStageDescriptorUniformBuffers;
            gpu_limits.maxDescriptorSetUpdateAfterBindSampledImages = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
            gpu_limits.maxDescriptorSetUpdateAfterBindStorageImages = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageImages;
            gpu_limits.maxDescriptorSetUpdateAfterBindStorageBuffers = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffers;
            gpu_limits.maxDescriptorSetUpdateAfterBindUniformBuffers = descriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers;
            gpu_limits.maxPerStageDescriptorUpdateAfterBindSampledImages = descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages;
            gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageBuffers = descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageBuffers;
            gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageImages = descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindStorageImages;
            gpu_limits.maxPerStageDescriptorUpdateAfterBindUniformBuffers =  descriptorIndexingProperties.maxPerStageDescriptorUpdateAfterBindUniformBuffers;
            gpu_limits.minImportedHostPointerAlignment = hostProperties.minImportedHostPointerAlignment;

            initialize();
        }

        GpuDeviceVulkan::~GpuDeviceVulkan()
        {
            crash_tracking_buffer.reset();
            setup_cb.reset();
            graphics_queue_setup_cb.reset();
            swapchain.reset();
            vkDeviceWaitIdle(device);
            for(auto& res : destroy_queue)
                res.frame_counter = 0xffff;
            release_resources();
            //vkDeviceWaitIdle(device);
   /*         vkDestroyDevice(device, nullptr);
            vkDestroyInstance(instance->instance,nullptr);*/
        }

        auto GpuDeviceVulkan::initialize()->void
        {
            for (int i = 0; i < 2; i++)
            {
                auto main_cmd = std::make_shared<GpuCommandBufferVulkan>(device, universe_queue);
                set_label((u64)main_cmd->handle, VK_OBJECT_TYPE_COMMAND_BUFFER, std::format("main_cmd_{}", i).c_str());
                auto present_cmd = std::make_shared<GpuCommandBufferVulkan>(device, universe_queue);
                set_label((u64)present_cmd->handle,VK_OBJECT_TYPE_COMMAND_BUFFER, std::format("present_cmd_{}",i).c_str());
                frames[i] = DeviceFrameVulkan(main_cmd, present_cmd, VK_NULL_HANDLE, VK_NULL_HANDLE);
            }

            immutable_samplers = create_samplers(device);
            setup_cb.reset(new GpuCommandBufferVulkan(device, transfer_queue));
            graphics_queue_setup_cb.reset(new GpuCommandBufferVulkan(device, universe_queue));
            set_label((u64)graphics_queue_setup_cb->handle,VK_OBJECT_TYPE_COMMAND_BUFFER, "graphics_queue_setup_cb");
            auto crash_buffer_desc = GpuBufferDesc::new_gpu_to_cpu(4, BufferUsageFlags::TRANSFER_DST);
            crash_tracking_buffer = create_buffer_impl(device, global_allocator, crash_buffer_desc,"crash tracing buffer");
            rt_scatch_buffer = create_ray_tracing_acceleration_scratch_buffer();
        }  

        std::shared_ptr<GpuTexture>  GpuDeviceVulkan::create_texture(const GpuTextureDesc& desc,const std::vector<ImageSubData>&  initial_data, const char* name)
        {
        #ifndef DS_PRODUCTION
             DS_LOG_INFO("creating an image: {}", desc);
        #endif
             auto vk_image = std::make_shared<GpuTextureVulkan>();
             vk_image->desc = desc;
             VkImage& image = vk_image->image;
             auto create_info = get_image_create_info(desc, !initial_data.empty());
             VK_CHECK_RESULT(vkCreateImage(device, &create_info, nullptr, &image));
             VkMemoryRequirements	requirements;
             vkGetImageMemoryRequirements(device, image, &requirements);
             VmaAllocationCreateInfo vbAllocCreateInfo = {};
             vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
             cb_mutex.lock();
             VK_CHECK_RESULT(vmaAllocateMemory(global_allocator, &requirements, &vbAllocCreateInfo, &vk_image->allocation, nullptr));
             VK_CHECK_RESULT(vkBindImageMemory(device, image, vk_image->allocation->GetMemory(), vk_image->allocation->GetOffset()));
             cb_mutex.unlock();
             uint32 total_initial_data_bytes = 0;
             if (!initial_data.empty())
             {
                 for (const auto& d : initial_data)
                     total_initial_data_bytes += d.size;
                 //uint32 total_initial_data_byte = std::accumulate(initial_data.begin(), initial_data.end(), 0, [&](ImageSubData	d)->uint32 {return d.data.size(); });
                 uint32 block_bytes = 1;
                 switch ( desc.format)
                 {
                 case PixelFormat::R8G8B8A8_UNorm:
                 case PixelFormat::R8G8B8A8_UNorm_sRGB:
                 case PixelFormat::R32G32B32A32_Float:
                 case PixelFormat::R16G16B16A16_Float:
                     block_bytes = 1;
                     break;
                 case    PixelFormat::BC1_UNorm_sRGB:
                 case    PixelFormat::BC1_UNorm:
                     block_bytes = 8;
                     break;
                 case PixelFormat::BC3_UNorm:
                 case PixelFormat::BC3_UNorm_sRGB :
                 case PixelFormat::BC5_UNorm:
                 case PixelFormat::BC5_SNorm:
                     block_bytes = 16;
                     break;
                 default:
                     block_bytes = 1;
                     break;
                 }
                
                 GpuBufferDesc buffer_desc = { total_initial_data_bytes, BufferUsageFlags::TRANSFER_SRC, CPU_TO_GPU };
                 auto img_buffer = std::static_pointer_cast<GpuBufferVulkan>(create_buffer(buffer_desc, "Image Initial Data Buffer", nullptr));
     
                 uint32 offset = 0;
                 void* mapped_data;
                 cb_mutex.lock();
                 vmaMapMemory(global_allocator, img_buffer->allocation, &mapped_data);
                 char* dstData = static_cast<char*>(mapped_data);
                 //memcpy(dstData, initial_data, desc.size);
                 std::vector< VkBufferImageCopy> buffer_img_copies;
                 for(int level =0 ;level < initial_data.size(); level++)
                 {
                     auto sub = initial_data[level];
                     assert(offset % block_bytes == 0 );
                     memcpy(dstData + offset, sub.data, sub.size);

                     VkImageSubresourceLayers subresourceRange = {};
                     subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                     subresourceRange.layerCount = 1;
                     subresourceRange.mipLevel = level;
                     subresourceRange.baseArrayLayer = 0;
                     VkBufferImageCopy	region = {};
                     region.bufferOffset = offset;
                     region.imageSubresource = subresourceRange;
                     region.imageExtent = VkExtent3D{ std::max(u32(1),desc.extent[0] >> level),  std::max(u32(1),desc.extent[1] >> level) , std::max(u32(1),desc.extent[2] >> level) };
                     buffer_img_copies.push_back(region);
                     offset += sub.size;
                 };

                 vmaUnmapMemory(global_allocator, img_buffer->allocation);
                 cb_mutex.unlock();

                 setup_cmd_buffer([&](VkCommandBuffer cmd) {
                     rhi::record_image_barrier(*this, cmd, ImageBarrier{
                         vk_image.get(),
                         AccessType::Nothing,
                         AccessType::TransferWrite,
                         ImageAspectFlags::COLOR,
                         true
                         }, setup_cb->family_index, setup_cb->family_index);

                     vkCmdCopyBufferToImage(cmd, img_buffer->handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, buffer_img_copies.size(), buffer_img_copies.data());
                     
                     rhi::record_image_barrier(*this, cmd, ImageBarrier{
                         vk_image.get(),
                         AccessType::TransferWrite,
                         AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                         ImageAspectFlags::COLOR,
                         false
                         }, setup_cb->family_index, setup_cb->family_index);
                     });

                 immediate_destroy_buffer(*img_buffer);			
             }
            set_name(vk_image.get(),name);
            return vk_image;
        }

        std::shared_ptr<GpuBuffer>   GpuDeviceVulkan::create_buffer(const GpuBufferDesc& desc_,const char* name,uint8* initial_data)
        {
            auto desc = desc_;
            if (initial_data)
                desc.usage |= (BufferUsageFlags::TRANSFER_DST);

            auto buffer = create_buffer_impl(device, global_allocator, desc, name);
            if (initial_data)
            {
                auto scratch_desc = GpuBufferDesc{
                    desc.size,
                    BufferUsageFlags::TRANSFER_SRC,
                    CPU_TO_GPU
                };
                
                auto scratch_buffer_name = fmt::format("initial data for {}", name);
                auto scratch_buffer = create_buffer_impl(device, global_allocator, scratch_desc, scratch_buffer_name.c_str());
                void* mapped_data;

                cb_mutex.lock();
                vmaMapMemory(global_allocator, scratch_buffer->allocation, &mapped_data);
                char* dstData = static_cast<char*>(mapped_data);
                memcpy(dstData, initial_data, desc.size);
                vmaUnmapMemory(global_allocator, scratch_buffer->allocation);
                cb_mutex.unlock();

                setup_cmd_buffer([&](VkCommandBuffer cmd) {
                    VkBufferCopy	region;
                    region.dstOffset = 0;
                    region.srcOffset = 0;
                    region.size = desc.size;
                    vkCmdCopyBuffer(cmd, scratch_buffer->handle, buffer->handle, 1, &region);
                    });
                
                immediate_destroy_buffer(*scratch_buffer);
            }
            set_name(buffer.get(), name);
            return buffer;
        }

        auto GpuDeviceVulkan::create_image_view(const GpuTextureViewDesc& view_desc, const GpuTextureDesc& image_desc, VkImage image) const -> std::shared_ptr<GpuTextureViewVulkan>
        {
            if( image_desc.format == PixelFormat::D32_Float && ! (view_desc.aspect_mask & ImageAspectFlags::DEPTH))
            {
                throw std::runtime_error(fmt::format("Depth-only resource used without the vk::ImageAspectFlags::DEPTH flag"));
                return nullptr;
            }
            auto create_info = GpuTextureVulkan::view_desc(image_desc, view_desc);
            create_info.image = image;
            VkImageView	imge_view;
            VK_CHECK_RESULT(vkCreateImageView(device, &create_info, nullptr, &imge_view));
            return std::make_shared<GpuTextureViewVulkan>(imge_view);
        }

        auto GpuDeviceVulkan::create_buffer_view(const GpuBufferViewDesc& view_desc, const GpuBufferDesc& buffer_desc, VkBuffer buffer) const -> std::shared_ptr<GpuBufferViewVulkan>
        {
            VkBufferViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
            viewInfo.buffer = buffer;
            viewInfo.format = pixel_format_2_vk(buffer_desc.format); 
            viewInfo.offset = view_desc.offset; 
            viewInfo.range = view_desc.range;  
            VkBufferView	buffer_view;
            VK_CHECK_RESULT(vkCreateBufferView(device, &viewInfo, nullptr, &buffer_view));
            return std::make_shared<GpuBufferViewVulkan>(buffer_view);
        }

        auto GpuDeviceVulkan::create_render_command_buffer(const char* name) -> std::shared_ptr<CommandBuffer>
        {
            auto main_cmd  = std::make_shared<GpuCommandBufferVulkan>(device, universe_queue);
            if(name)
                set_label((u64)main_cmd->handle, VK_OBJECT_TYPE_COMMAND_BUFFER, name);
            return main_cmd;
        }
        auto GpuDeviceVulkan::create_swapchain(SwapchainDesc desc,void* window_handle) -> std::shared_ptr<Swapchain>
        {
           swapchain = std::make_shared<SwapchainVulkan>(this, desc, window_handle);
           return swapchain;
        }

        auto GpuDeviceVulkan::create_samplers(VkDevice device) -> std::unordered_map<GpuSamplerDesc, VkSampler>
        {
            //auto texel_filters = { VK_FILTER_NEAREST, VkFilter::VK_FILTER_LINEAR };
            //auto mipmaps_modes = { VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_NEAREST,VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR };
            //auto address_modes = { VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE };
            auto texel_filters = {TexelFilter::NEAREST, TexelFilter ::LINEAR};
            auto mipmaps_modes = { SamplerMipmapMode::NEAREST, SamplerMipmapMode::LINEAR };
            auto address_modes = { SamplerAddressMode::REPEAT, SamplerAddressMode ::CLAMP_TO_EDGE};
            auto result = std::unordered_map<GpuSamplerDesc, VkSampler>();
            for (auto& texel_filter : texel_filters)
            {
                for (auto& mipmap_mode : mipmaps_modes)
                {
                    for (auto& address_mode : address_modes)
                    {
                        auto anisotropy_enable = texel_filter_mode_2_vk(texel_filter) == VK_FILTER_LINEAR;
                        VkSamplerCreateInfo create_info = {};
                        create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                        create_info.minFilter = texel_filter_mode_2_vk(texel_filter);
                        create_info.magFilter = texel_filter_mode_2_vk(texel_filter);
                        create_info.mipmapMode = mipmap_mode_2_vk(mipmap_mode);
                        create_info.anisotropyEnable = anisotropy_enable;
                        create_info.addressModeU = address_mode_2_vk(address_mode);
                        create_info.addressModeV = address_mode_2_vk(address_mode);
                        create_info.addressModeW = address_mode_2_vk(address_mode);
                        create_info.maxAnisotropy = 16.0;
                        create_info.maxLod = VK_LOD_CLAMP_NONE;
                        VkSampler	sampler;
                        vkCreateSampler(device, &create_info, nullptr, &sampler);
                        auto sampler_desc = GpuSamplerDesc{ texel_filter, mipmap_mode, address_mode };
                        result[sampler_desc] = sampler;
                    }
                }
            }
            return result;
        }

 //       auto GpuDeviceVulkan::get_min_offset_alignment(const GpuBufferDesc& desc) -> u64
 //       {
 //           uint64_t alignment = 1u;
 ///*           if (has_flag(desc., BindFlag::CONSTANT_BUFFER))
 //           {
 //               alignment = std::max(alignment, gpu_limits.minUniformBufferOffsetAlignment);
 //           }
 //           if (desc->format == Format::UNKNOWN)
 //           {
 //               alignment = std::max(alignment, gpu_limits.minStorageBufferOffsetAlignment);
 //           }
 //           else
 //           {
 //               alignment = std::max(alignment, gpu_limits.minTexelBufferOffsetAlignment);
 //           }*/
 //           return alignment;
 //       }

        //overide func
        auto GpuDeviceVulkan::create_render_pass(const RenderPassDesc& desc,const char* name) -> std::shared_ptr<RenderPass>
        {
            std::vector<VkAttachmentDescription>	renderpass_attachments;
            for (auto attachment : desc.color_attachments)
            {
                VkAttachmentDescription attach_des = to_vk(attachment, VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                renderpass_attachments.push_back(attach_des);
            }

            if (desc.depth_attachment.has_value())
            {
                renderpass_attachments.push_back(to_vk(desc.depth_attachment.value(), VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL));
            }

            std::vector<VkAttachmentReference>	color_attachment_refs;
            for (auto attachment = 0; attachment < desc.color_attachments.size(); attachment++)
            {
                color_attachment_refs.emplace_back(VkAttachmentReference{ (u32)attachment , VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
            }
            VkAttachmentReference	depth_attachment_ref;
            depth_attachment_ref.attachment = desc.color_attachments.size();
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;

            VkSubpassDescription subpass_description = {};
            subpass_description.pColorAttachments = color_attachment_refs.data();
            subpass_description.colorAttachmentCount = color_attachment_refs.size();
            subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            if (desc.depth_attachment.has_value())
                subpass_description.pDepthStencilAttachment = &depth_attachment_ref;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = renderpass_attachments.size();
            renderPassInfo.pAttachments = renderpass_attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass_description;

            VkRenderPass	render_pass;
            vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass);
            if(name != nullptr)
                set_label((u64)render_pass, VK_OBJECT_TYPE_RENDER_PASS, name);
            auto pass = std::make_shared<RenderPassVulkan>();
            pass->render_pass = render_pass;
            pass->framebuffer_cache = FrameBufferCache(render_pass, desc.color_attachments, desc.depth_attachment);
            return pass;
        }

        auto GpuDeviceVulkan::get_sampler(const GpuSamplerDesc& desc) const -> VkSampler
        {
            auto it = immutable_samplers.find(desc);
            if (it != immutable_samplers.end()) {
                return it->second;
            }
            else {
                throw std::runtime_error(fmt::format("sampler not found:"));
            }
            return nullptr;
        }

        auto GpuDeviceVulkan::create_descriptor_set(const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name) -> std::shared_ptr<DescriptorSet>
        {
            std::vector<VkDescriptorBindingFlags> set_binding_flags(descriptors.size(), VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
            for (auto i = 0; i < descriptors.size(); i++)
            {
                if (descriptors.at(i).dimensionality.ty == rhi::DescriptorDimensionality::RuntimeArray){
                    set_binding_flags[i] |= VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
                }
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {};
            binding_flags_create_info.pBindingFlags = set_binding_flags.data();
            binding_flags_create_info.bindingCount = set_binding_flags.size();
            binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            VkDescriptorSetLayoutCreateInfo layout_create_info = {};
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (const auto& [binding_idx,descriptor] : descriptors)
            {
                VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                u32 descriptorCount = 1;
                switch (descriptor.ty)
                {
                case DescriptorType::UNIFORM_BUFFER:
                case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
                case DescriptorType::UNIFORM_TEXEL_BUFFER:
                case DescriptorType::STORAGE_IMAGE:
                case DescriptorType::STORAGE_BUFFER:
                case DescriptorType::STORAGE_BUFFER_DYNAMIC:
                {
                    if (descriptor.dimensionality.ty == DescriptorDimensionality::RuntimeArray)
                    {
                         if (descriptor.ty == DescriptorType::STORAGE_BUFFER )
                             descriptorCount = std::min<u32>(gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageBuffers,max_bindless_storage_buffers()) / (MAX_SET_COUNT * descriptors.size());
                         else if( descriptor.ty == DescriptorType::STORAGE_IMAGE)
                             descriptorCount = std::min<u32>(gpu_limits.maxPerStageDescriptorUpdateAfterBindStorageImages,max_bindless_storage_images()) / (MAX_SET_COUNT * descriptors.size());
                         else
                             descriptorCount = std::min<u32>(gpu_limits.maxPerStageDescriptorUpdateAfterBindUniformBuffers,max_bindless_uniform_textel_buffers()) / (MAX_SET_COUNT * descriptors.size());
                    }
                    else
                        descriptorCount = 1;
                    if (descriptor.ty == DescriptorType::UNIFORM_BUFFER || descriptor.ty == DescriptorType::UNIFORM_BUFFER_DYNAMIC)
                        descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    else if (descriptor.ty == DescriptorType::UNIFORM_TEXEL_BUFFER)
                        descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                    else if (descriptor.ty == DescriptorType::STORAGE_IMAGE)
                        descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    else if (descriptor.ty == DescriptorType::STORAGE_BUFFER)
                    {
                        descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    }
                }break;
                case DescriptorType::SAMPLER:
                    descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
                    break;
                case DescriptorType::SAMPLED_IMAGE:
                {
                    switch (descriptor.dimensionality.ty)
                    {
                    case DescriptorDimensionality::Single:
                        descriptorCount = 1;
                        break;
                    case DescriptorDimensionality::Array:
                        descriptorCount = descriptor.dimensionality.value;
                        break;
                    case DescriptorDimensionality::RuntimeArray:
                        descriptorCount = std::min<u32>(gpu_limits.maxPerStageDescriptorUpdateAfterBindSampledImages,max_bindless_sampled_images()) / MAX_SET_COUNT;
                        break;
                    default:
                        break;
                    }
                    descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                }
                    break;
                case DescriptorType::ACCELERATION_STRUCTURE_KHR:
                    descriptor_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    break;
                default:
                    descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                }
                VkDescriptorSetLayoutBinding layout_binding = {};
                layout_binding.descriptorCount = descriptorCount;
                layout_binding.descriptorType = descriptor_type;
                layout_binding.stageFlags = VK_SHADER_STAGE_ALL;
                layout_binding.binding = binding_idx;
                bindings.push_back(layout_binding);
            }
            layout_create_info.pBindings = bindings.data();
            layout_create_info.bindingCount = bindings.size();
            layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_create_info.pNext = &binding_flags_create_info;
            layout_create_info.flags = rhi::DescriptorSetLayoutCreateFlags::UPDATE_AFTER_BIND_POOL;
            VkDescriptorSetLayout   descriptor_set_layout;
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));

            std::vector<VkDescriptorPoolSize>   descriptor_pool_sizes;
            for (const auto& binding : bindings)
            {
                descriptor_pool_sizes.push_back({ binding.descriptorType, binding.descriptorCount });
            }

            VkDescriptorPoolCreateInfo  pool_create_info = {};
            pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_create_info.poolSizeCount = descriptor_pool_sizes.size();
            pool_create_info.pPoolSizes = descriptor_pool_sizes.data();
            pool_create_info.maxSets = 1;
            pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

            VkDescriptorPool    descriptor_pool;
            VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));

            auto variable_descriptor_count  = max_bindless_storage_images();
            VkDescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_allocate_info = {};
            variable_descriptor_count_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variable_descriptor_count_allocate_info.pDescriptorCounts = &variable_descriptor_count;
            variable_descriptor_count_allocate_info.descriptorSetCount = 0;
            variable_descriptor_count_allocate_info.pNext = nullptr;

            VkDescriptorSet set;
            VkDescriptorSetAllocateInfo set_alloc_info = {};
            set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            set_alloc_info.descriptorPool = descriptor_pool;
            set_alloc_info.pSetLayouts = &descriptor_set_layout;
            set_alloc_info.pNext = &variable_descriptor_count_allocate_info;
            set_alloc_info.descriptorSetCount = 1;
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &set_alloc_info, &set));
            if(name != nullptr)
            {
                set_label((u64)set, VK_OBJECT_TYPE_DESCRIPTOR_SET, name);
            }
            return std::make_shared<VulkanDescriptorSet>(set, descriptor_pool);
        }
        //frame descriptor
        auto GpuDeviceVulkan::create_descriptor_set(GpuBuffer* dynamic_constants, const std::unordered_map<u32, DescriptorInfo>& descriptors,const char* name) -> std::shared_ptr<DescriptorSet>
        {
            std::vector<VkDescriptorBindingFlags> set_binding_flags(descriptors.size(), VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
            for (auto i = 0; i < descriptors.size(); i++)
            {
                if( descriptors.at(i).dimensionality.ty == rhi::DescriptorDimensionality::RuntimeArray)
                    set_binding_flags[i] |= VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
            }
            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info = {};
            binding_flags_create_info.pBindingFlags = set_binding_flags.data();
            binding_flags_create_info.bindingCount = set_binding_flags.size();
            binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            VkDescriptorSetLayoutCreateInfo layout_create_info = {};
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (const auto& [binding_idx,descriptor] : descriptors)
            {
               VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
               switch (descriptor.ty)
               {
               case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
                    descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    break;
               case DescriptorType::STORAGE_BUFFER_DYNAMIC:
                   descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                   break;
               case DescriptorType::SAMPLER:
                   descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
                   break;
               case DescriptorType::SAMPLED_IMAGE:
                   descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                   break;
               case DescriptorType::STORAGE_BUFFER:
                   descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                   break;
               case DescriptorType::UNIFORM_BUFFER:
                   descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                   break;
               case DescriptorType::ACCELERATION_STRUCTURE_KHR:
                   descriptor_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                   break;
               default:
                   descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                   break;
               }
               VkDescriptorSetLayoutBinding layout_binding = {};
               layout_binding.descriptorCount = 1;
               layout_binding.descriptorType = descriptor_type;
               layout_binding.stageFlags = VK_SHADER_STAGE_ALL;
               layout_binding.binding = binding_idx;
               bindings.push_back(layout_binding);
            }
            layout_create_info.pBindings = bindings.data();
            layout_create_info.bindingCount = bindings.size();
            layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_create_info.pNext = &binding_flags_create_info;

            VkDescriptorSetLayout   descriptor_set_layout;
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout));

            std::vector<VkDescriptorPoolSize>   descriptor_pool_sizes;
            for (const auto& binding : bindings)
            {
                descriptor_pool_sizes.push_back({binding.descriptorType, 1});
            }

            VkDescriptorPoolCreateInfo  pool_create_info = {};
            pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_create_info.poolSizeCount = descriptor_pool_sizes.size();
            pool_create_info.pPoolSizes = descriptor_pool_sizes.data();
            pool_create_info.maxSets = 1;

            VkDescriptorPool    descriptor_pool;
            VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));

            VkDescriptorSet set;
            VkDescriptorSetAllocateInfo set_alloc_info = {};
            set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            set_alloc_info.descriptorPool = descriptor_pool;
            set_alloc_info.pSetLayouts = &descriptor_set_layout;
            set_alloc_info.descriptorSetCount = 1;
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &set_alloc_info,&set));
            if(name != nullptr)
            {
                set_label((u64)set, VK_OBJECT_TYPE_DESCRIPTOR_SET, name);
            }
            std::vector<VkWriteDescriptorSet>   descriptor_set_writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            buffer_infos.reserve(bindings.size());
            //buffer info
            for(const auto& binding : bindings)
            {
                if(binding.descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC && binding.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                    continue;
                buffer_infos.emplace_back();
                auto&  buffer_info = buffer_infos.back();
                buffer_info = {};
                int range = 1;
                switch (binding.descriptorType)
                {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                    range = MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH;
                break;
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                    range = MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES;
                break;
                default:
                    break;
                }
                buffer_info.buffer = static_cast<GpuBufferVulkan*>(dynamic_constants)->handle;
                buffer_info.range = range;
                buffer_info.offset = 0;


                VkWriteDescriptorSet    write_set = {};
                write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write_set.dstBinding = binding.binding;
                write_set.descriptorType = binding.descriptorType;
                write_set.dstSet = set;
                write_set.pBufferInfo = &buffer_info;
                write_set.descriptorCount = 1;
                descriptor_set_writes.push_back(write_set);
            }   
            vkUpdateDescriptorSets(device, descriptor_set_writes.size(), descriptor_set_writes.data(), 0, nullptr);
            return std::make_shared<VulkanDescriptorSet>(set, descriptor_pool);
        }

        //TODO
        auto GpuDeviceVulkan::bind_descriptor_set(CommandBuffer* cb,GpuPipeline* pipeline,  std::vector<DescriptorSetBinding>& bindings, uint32 set_index) -> void
        {
            auto pipeline_vk = reinterpret_cast<PipelineVulkan*>(pipeline);
            if(set_index >= pipeline_vk->set_layout_info.size() ) return;
            auto shader_set_info = pipeline_vk->set_layout_info.at(set_index);

            VkDescriptorPoolCreateInfo  pool_create_info = {};
            pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_create_info.poolSizeCount = pipeline_vk->descriptor_pool_sizes.size();
            pool_create_info.pPoolSizes = pipeline_vk->descriptor_pool_sizes.data();
            pool_create_info.maxSets = 1;

            VkDescriptorPool    descriptor_pool;
            VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool));
            defer_release(descriptor_pool);

            VkDescriptorSet descriptor_set;
            VkDescriptorSetAllocateInfo set_alloc_info = {};
            set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            set_alloc_info.descriptorPool = descriptor_pool;
            set_alloc_info.pSetLayouts = &pipeline_vk->descriptor_set_layouts[set_index];
            set_alloc_info.descriptorSetCount = 1;
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &set_alloc_info, &descriptor_set));
            
            std::vector<uint32> dynamic_offsets = {};
            std::vector<VkWriteDescriptorSet>   descriptor_writes;
            std::vector<VkDescriptorImageInfo> image_infos;
            std::vector<VkDescriptorImageInfo> image_array_infos;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accel_infos;

            image_infos.reserve(bindings.size());
            buffer_infos.reserve(bindings.size());
            //TODO:
            for (auto binding_idx = 0; binding_idx < bindings.size(); binding_idx++)
            {
                if (shader_set_info.find(binding_idx) != shader_set_info.end())
                {
                    auto descriptorType = shader_set_info[binding_idx];
                    descriptor_writes.emplace_back();
                    auto& write_set = descriptor_writes.back();
                    write_set = {};
                    write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write_set.dstBinding = binding_idx;
                    write_set.dstSet = descriptor_set;
                    write_set.dstArrayElement = 0;
                    write_set.descriptorCount = 1;
                    auto& binding = bindings[binding_idx];
                    switch (binding.ty)
                    {
                    case DescriptorSetBinding::Type::Image:
                    {
                        auto& image = binding.Image();
                        //if(image.image_layout   == IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                        //    write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        //else if(image.image_layout == IMAGE_LAYOUT_GENERAL)
                        //    write_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                        write_set.descriptorType = descriptorType;
                        image_infos.emplace_back();
                        write_set.pImageInfo = &image_infos.back();
                        image_infos.back() = {};
                        image_infos.back().imageView = reinterpret_cast<GpuTextureViewVulkan*>(image.view)->img_view;
                        image_infos.back().imageLayout = image_layout_2_vk(image.image_layout); 
                    }
                        break;
                    case DescriptorSetBinding::Type::ImageArray:
                    {
                        auto& imginfos = binding.ImageArray();
                        for (auto& img : imginfos)
                        {
                            image_array_infos.emplace_back(VkDescriptorImageInfo{});
                            auto& info = image_array_infos.back();
                            info.imageLayout = image_layout_2_vk(img.image_layout);
                            info.imageView = reinterpret_cast<GpuTextureViewVulkan*>(img.view)->img_view;
                        }
               /*         if (imginfos[0].image_layout == IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                            write_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        else if (imginfos[0].image_layout == IMAGE_LAYOUT_GENERAL)*/
                        write_set.descriptorType = descriptorType;
                        write_set.pImageInfo = image_array_infos.data();
                        write_set.descriptorCount = image_array_infos.size();
                    }
                        break;
                    case DescriptorSetBinding::Type::Buffer:
                    {
                        auto& buf = binding.Buffer();
                        buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                        auto vk_buf = static_cast<GpuBufferVulkan*>(buf.buffer);
                        auto& buffer_info = buffer_infos.back();
                        buffer_info.buffer = vk_buf->handle;
                        buffer_info.range = VK_WHOLE_SIZE;
                        buffer_info.offset = 0;
                        write_set.descriptorType = descriptorType;
                        if(descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                            descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                        {
                            auto buffer_view = reinterpret_cast<GpuBufferViewVulkan*>(buf.buffer->view(this, rhi::GpuBufferViewDesc()).get());
                            if (!buffer_view)
                            {
                                DS_LOG_CRITICAL("TEXEL_BUFFER is null bindid:{}", binding_idx);
                            }
                            write_set.pTexelBufferView = &buffer_view->buf_view;
                        }
                        else
                            write_set.pBufferInfo = &buffer_info;
                    }
                        break;
                    case DescriptorSetBinding::Type::DynamicBuffer:
                    {
                        auto& [dy_buf, offset] = binding.DynamicBuffer();
                        dynamic_offsets.push_back(offset);
            
                        buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                        auto& buffer_info = buffer_infos.back();
                        buffer_info.buffer = static_cast<GpuBufferVulkan*>(dy_buf)->handle;
                        buffer_info.range = MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH;
                        buffer_info.offset = 0;

                        write_set.descriptorType = descriptorType;
                        write_set.pBufferInfo = &buffer_info;
                    }
                        break;
                    case DescriptorSetBinding::Type::DynamicStorageBuffer:
                    {
                        auto& [dy_buf, offset] = binding.DynamicBuffer();
                        dynamic_offsets.push_back(offset);

                        buffer_infos.emplace_back(VkDescriptorBufferInfo{});
                        auto& buffer_info = buffer_infos.back();
                        buffer_info.buffer = static_cast<GpuBufferVulkan*>(dy_buf)->handle;
                        buffer_info.range = MAX_DYNAMIC_CONSTANTS_STORAGE_BUFFER_BYTES;
                        buffer_info.offset = 0;

                        write_set.descriptorType = descriptorType;
                        write_set.pBufferInfo = &buffer_info;
                    }
                        break;
                    case DescriptorSetBinding::Type::RayTracingAcceleration:
                    {
                        write_set.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                        auto acc = binding.RayTracingAcceleration();
                        assert(acc);
                        accel_infos.emplace_back();
                        accel_infos.back() = {};
                        accel_infos.back().sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                        accel_infos.back().accelerationStructureCount = 1;
                        accel_infos.back().pAccelerationStructures = &reinterpret_cast<GpuRayTracingAccelerationVulkan*>(acc)->acc;
                        write_set.pNext = &accel_infos.back();

                    }
                        break;
                    }
                }
            }

            vkUpdateDescriptorSets(device, descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdBindDescriptorSets(cmd, pipeline_vk->pipeline_bind_point, pipeline_vk->pipeline_layout, set_index, 1, &descriptor_set, dynamic_offsets.size(), dynamic_offsets.data());
        }

        auto GpuDeviceVulkan::bind_descriptor_set(CommandBuffer* cb, GpuPipeline* pipeline, uint32 set_idx,DescriptorSet* set,u32 dynamic_offset_count, u32* dynamic_offset) -> void
        {
            auto vk_cmd = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto vk_set = reinterpret_cast<VulkanDescriptorSet*>(set)->descriptor_set;
            auto pipeline_vk = static_cast<PipelineVulkan*>(pipeline);
            vkCmdBindDescriptorSets(vk_cmd, pipeline_vk->pipeline_bind_point, pipeline_vk->pipeline_layout, set_idx, 1, &vk_set, dynamic_offset_count, dynamic_offset);
        }

        auto GpuDeviceVulkan::bind_pipeline(CommandBuffer* cb, GpuPipeline* pipeline) -> void
        {
            auto vk_pipe = reinterpret_cast<PipelineVulkan*>(pipeline);
            auto cmd = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdBindPipeline(cmd, vk_pipe->pipeline_bind_point, vk_pipe->pipeline);
        }

        auto GpuDeviceVulkan::push_constants(CommandBuffer* cb, GpuPipeline* pipeline, u32 offset, u8* constants, u32 size_) -> void
        {
            auto vk_pipe = reinterpret_cast<PipelineVulkan*>(pipeline);
            auto cmd = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdPushConstants(cmd, vk_pipe->pipeline_layout, VK_SHADER_STAGE_ALL_GRAPHICS, offset, size_, constants);
        }

        auto GpuDeviceVulkan::create_compute_pipeline(const CompiledShaderCode& spirv, const ComputePipelineDesc& desc) -> std::shared_ptr<ComputePipeline>
        {
            auto [descriptor_set_layouts, set_layout_info] = create_descriptor_set_layouts(*this, std::move(get_descriptor_sets(spirv)), VK_SHADER_STAGE_COMPUTE_BIT, desc.descriptor_set_opts);

            VkPipelineLayoutCreateInfo layout_create_info = {};
            layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_create_info.pSetLayouts = descriptor_set_layouts.data();
            layout_create_info.setLayoutCount = (uint32_t)descriptor_set_layouts.size();
            VkPushConstantRange push_constant_ranges = { VK_SHADER_STAGE_COMPUTE_BIT, 0, desc.push_constant_bytes};
            if (desc.push_constant_bytes > 0)
            { 
               layout_create_info.pPushConstantRanges = &push_constant_ranges;
               layout_create_info.pushConstantRangeCount = 1;
            }
            VkShaderModule shader_module;
            VkShaderModuleCreateInfo shader_create_info = {};
            shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shader_create_info.pCode = (uint32*)spirv.codes.data();
            shader_create_info.codeSize = spirv.codes.size();
            VK_CHECK_RESULT(vkCreateShaderModule(device, &shader_create_info, nullptr, &shader_module));

            VkPipelineLayout pipeline_layout;
            VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layout_create_info, nullptr, &pipeline_layout));


            const auto entry_name = desc.source.entry;
            VkPipelineShaderStageCreateInfo stage_create_info = {};
            stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage_create_info.module = shader_module;
            stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            stage_create_info.pName = entry_name.c_str();

            VkComputePipelineCreateInfo pipeline_info = {};
            pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipeline_info.stage = stage_create_info;
            pipeline_info.layout = pipeline_layout;

            VkPipeline pipeline;
            VK_CHECK_RESULT(vkCreateComputePipelines(device, 0, 1, &pipeline_info, nullptr, &pipeline));
            if(desc.name.data() != nullptr)
            {
                const auto name = desc.name.data();
                set_label(reinterpret_cast<u64>(pipeline),VK_OBJECT_TYPE_PIPELINE,name);
                set_label(reinterpret_cast<u64>(pipeline_layout),VK_OBJECT_TYPE_PIPELINE_LAYOUT,name);
                set_label(reinterpret_cast<u64>(shader_module),VK_OBJECT_TYPE_SHADER_MODULE,name);
            }
            std::vector<VkDescriptorPoolSize> descriptor_pool_sizes;
            for (auto bindings : set_layout_info)
            {
                for (auto bd : bindings) {
                   auto it =  std::find_if(descriptor_pool_sizes.begin(), descriptor_pool_sizes.end(), [bd](const VkDescriptorPoolSize& item) {
                        return bd.second == item.type;
                   });
                   if( it!= descriptor_pool_sizes.end())
                       it->descriptorCount += 1;
                   else
                       descriptor_pool_sizes.push_back({
                        bd.second,
                        1
                       });
                }
            }

            auto compute_pipeline = std::make_shared<ComputePipelineVulkan>(pipeline_layout, pipeline, std::move(set_layout_info), std::move(descriptor_pool_sizes), std::move(descriptor_set_layouts),VK_PIPELINE_BIND_POINT_COMPUTE) ;
            compute_pipeline->group_size = get_cs_local_size_from_spirv(spirv);
            compute_pipeline->ty = GpuPipeline::PieplineType::Compute;
            return compute_pipeline;
        }

        auto GpuDeviceVulkan::create_raster_pipeline(const std::vector<PipelineShader>& shaders, const RasterPipelineDesc& desc) -> std::shared_ptr<RasterPipeline> 
        {
            std::vector<StageDescriptorSetLayouts> stage_layouts;
            for(const auto& shader : shaders){
                stage_layouts.emplace_back(get_descriptor_sets(shader.code));
            }

            auto [descriptor_set_layouts, set_layout_info] = create_descriptor_set_layouts(*this, std::move(merge_shader_stage_layouts(stage_layouts)), VK_SHADER_STAGE_ALL_GRAPHICS, desc.descriptor_set_opts);
            
            VkPipelineLayoutCreateInfo layout_create_info = {};
            layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_create_info.pSetLayouts = descriptor_set_layouts.data();
            layout_create_info.setLayoutCount = (uint32_t)descriptor_set_layouts.size();
            VkPushConstantRange push_constant_ranges = { VK_SHADER_STAGE_ALL_GRAPHICS, 0, desc.push_constants_bytes };
            if (desc.push_constants_bytes > 0)
            {
                layout_create_info.pPushConstantRanges = &push_constant_ranges;
                layout_create_info.pushConstantRangeCount = 1;
            }

            VkPipelineLayout pipeline_layout;
            VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layout_create_info, nullptr, &pipeline_layout));

            std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescription = {};
            u32 vertex_stride = 0;
            std::vector< VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
            for (const auto& shader : shaders)
            {
                VkShaderModule shader_module;
                VkShaderModuleCreateInfo shader_create_info = {};
                shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                shader_create_info.pCode = (uint32*)shader.code.codes.data();
                shader_create_info.codeSize = shader.code.codes.size();
                VK_CHECK_RESULT(vkCreateShaderModule(device, &shader_create_info, nullptr, &shader_module));

                VkShaderStageFlagBits  stage;
                switch ( shader.desc.stage)
                {
                case ShaderPipelineStage::Vertex:
                    stage = VK_SHADER_STAGE_VERTEX_BIT;
                    if( desc.vextex_attribute )
                        vertexInputAttributeDescription = get_vertex_input_attribute_desc(shader.code, vertex_stride);
                    break;
                case ShaderPipelineStage::Geometry:
                    stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                    break;
                case ShaderPipelineStage::Pixel:
                    stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    break;
                default:
                    break;
                }     
                auto entry_name = shader.desc.source.entry.c_str();
                VkPipelineShaderStageCreateInfo stage_create_info = {};
                stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_create_info.module = shader_module;
                stage_create_info.stage = stage;
                stage_create_info.pName = entry_name;
                shader_stage_create_infos.emplace_back(stage_create_info);
                set_label(reinterpret_cast<u64>(shader_module),VK_OBJECT_TYPE_SHADER_MODULE, shader.desc.source.path.c_str());
            }

            VkVertexInputBindingDescription vertexBindingDescription = {};
            if (vertex_stride > 0)
            {
                vertexBindingDescription.binding = 0;
                vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                vertexBindingDescription.stride = vertex_stride;
            }

            VkPipelineVertexInputStateCreateInfo    vertex_input_state_info = {};
            vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_info.pNext = NULL;
            if( desc.vextex_attribute )
            {
                vertex_input_state_info.vertexBindingDescriptionCount = vertex_stride > 0 ? 1 : 0;
                vertex_input_state_info.pVertexBindingDescriptions = vertex_stride > 0 ? &vertexBindingDescription : nullptr;
                vertex_input_state_info.vertexAttributeDescriptionCount = vertex_stride > 0 ? uint32_t(vertexInputAttributeDescription.size()) : 0;
                vertex_input_state_info.pVertexAttributeDescriptions = vertex_stride > 0 ? vertexInputAttributeDescription.data() : nullptr;
            }

            VkPipelineInputAssemblyStateCreateInfo vertex_input_assembly_state_info = {};
            vertex_input_assembly_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            vertex_input_assembly_state_info.pNext = NULL;
#ifdef DS_PLATFORM_MACOS
            vertex_input_assembly_state_info.primitiveRestartEnable = VK_FALSE;
#else
            vertex_input_assembly_state_info.primitiveRestartEnable = VK_FALSE;
#endif
            vertex_input_assembly_state_info.topology = to_vk(desc.primitive_type);// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewport_state_info = {};
            viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport_state_info.pNext = NULL;
            viewport_state_info.viewportCount = 1;
            viewport_state_info.scissorCount = 1;
            viewport_state_info.pScissors = NULL;
            viewport_state_info.pViewports = NULL;
            // dynamic viewpor and scissor update, not really specify when creation;
            std::vector<VkDynamicState>  dynamicStateDescriptors;
            dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_VIEWPORT);
            dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_SCISSOR);
            
             if (desc.line_width < 0.0f)
             {
                 dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
             }
             
             if (desc.depth_bias_enabled)
                 dynamicStateDescriptors.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
            VkPipelineDynamicStateCreateInfo    dynamic_state_info = {};
            dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_info.pDynamicStates = dynamicStateDescriptors.data();
            dynamic_state_info.dynamicStateCount = dynamicStateDescriptors.size();
            
            VkPipelineRasterizationStateCreateInfo rasterization_info = {};
            rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterization_info.polygonMode = to_vk(desc.polygon_mode);
            rasterization_info.cullMode = to_vk(desc.cull_mode);
            rasterization_info.frontFace = (VkFrontFace)desc.face_order;//VK_FRONT_FACE_CLOCKWISE;//VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterization_info.depthClampEnable = VK_FALSE;
            rasterization_info.rasterizerDiscardEnable = desc.discard_enable;
            rasterization_info.depthBiasEnable = (desc.depth_bias_enabled ? VK_TRUE : VK_FALSE);
            rasterization_info.depthBiasConstantFactor = 0;
            rasterization_info.depthBiasClamp = 0;
            rasterization_info.depthBiasSlopeFactor = 0;
            rasterization_info.lineWidth = desc.line_width;
            rasterization_info.pNext = NULL;

            VkPipelineMultisampleStateCreateInfo multisample_state_info = {};
            multisample_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample_state_info.pNext = NULL;
            multisample_state_info.pSampleMask = NULL;
            multisample_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;//msaaFlag;
            multisample_state_info.sampleShadingEnable = VK_FALSE;
            multisample_state_info.alphaToCoverageEnable = VK_FALSE;
            multisample_state_info.alphaToOneEnable = VK_FALSE;
            multisample_state_info.minSampleShading = 0.0;

            VkStencilOpState noop_stencil_state = {};
            noop_stencil_state.failOp = VK_STENCIL_OP_KEEP;
            noop_stencil_state.passOp = VK_STENCIL_OP_KEEP;
            noop_stencil_state.compareOp = VK_COMPARE_OP_ALWAYS;
            noop_stencil_state.depthFailOp = VK_STENCIL_OP_KEEP;

            VkPipelineDepthStencilStateCreateInfo depth_state_info = {};
           	depth_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth_state_info.pNext = NULL;
            depth_state_info.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
            depth_state_info.depthWriteEnable = desc.depth_write? VK_TRUE : VK_FALSE;
            depth_state_info.depthCompareOp = static_cast<VkCompareOp>(desc.depth_compare_op);//VK_COMPARE_OP_GREATER_OR_EQUAL;//VK_COMPARE_OP_LESS_OR_EQUAL;//
            depth_state_info.depthBoundsTestEnable = VK_FALSE;
            depth_state_info.stencilTestEnable = VK_FALSE;
            depth_state_info.back = noop_stencil_state;
            depth_state_info.front = noop_stencil_state;
            depth_state_info.minDepthBounds = 0;
            depth_state_info.maxDepthBounds = 1.0;

            VkPipelineColorBlendStateCreateInfo color_blend_state = {};
            color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state.pNext = NULL;
            color_blend_state.flags = 0;
             std::vector<VkPipelineColorBlendAttachmentState> blendAttachState;

             auto render_pass = dynamic_cast<RenderPassVulkan*>(desc.render_pass.get());
             blendAttachState.resize(render_pass->framebuffer_cache.color_attachment_count);
             for (unsigned int i = 0; i < blendAttachState.size(); i++)
             {
                 blendAttachState[i] = VkPipelineColorBlendAttachmentState();
                 blendAttachState[i].colorWriteMask = 0x0f;
                 blendAttachState[i].alphaBlendOp = VK_BLEND_OP_ADD;
                 blendAttachState[i].colorBlendOp = VK_BLEND_OP_ADD;

                 if (desc.blend_enabled)
                 {
                    blendAttachState[i].blendEnable = VK_TRUE;
                    blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    if(desc.blend_mode == BlendMode::SrcAlphaOneMinusSrcAlpha)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                        blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                        blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    }
                    else if(desc.blend_mode == BlendMode::SrcAlphaOne)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                        blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                    }
                    else if(desc.blend_mode == BlendMode::ZeroSrcColor)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
                    }
                    else if(desc.blend_mode == BlendMode::OneZero)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                    }
                    else if (desc.blend_mode == BlendMode::OneOneMinusSrcAlpha)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                    }
                    else if (desc.blend_mode == BlendMode::OneOne)
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                    }
                    else
                    {
                        blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                        blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                    }
                 } 
                 else 
                 {
                     blendAttachState[i].blendEnable = VK_FALSE;
                     blendAttachState[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                     blendAttachState[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                     blendAttachState[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                     blendAttachState[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                 }
             }
             color_blend_state.attachmentCount = static_cast<uint32_t>(blendAttachState.size());
             color_blend_state.pAttachments = blendAttachState.data();
             color_blend_state.logicOpEnable = VK_FALSE;
             //color_blend_state.logicOp = VK_LOGIC_OP_NO_OP;
             color_blend_state.blendConstants[0] = 1.0f;
             color_blend_state.blendConstants[1] = 1.0f;
             color_blend_state.blendConstants[2] = 1.0f;
             color_blend_state.blendConstants[3] = 1.0f;

            VkGraphicsPipelineCreateInfo graphic_pipeline_info = {};
            graphic_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            graphic_pipeline_info.layout = pipeline_layout;
            graphic_pipeline_info.pVertexInputState = &vertex_input_state_info;
            graphic_pipeline_info.pInputAssemblyState = &vertex_input_assembly_state_info;
            graphic_pipeline_info.pRasterizationState = &rasterization_info;
            graphic_pipeline_info.pColorBlendState = &color_blend_state;
            graphic_pipeline_info.pMultisampleState = &multisample_state_info;
            graphic_pipeline_info.pDynamicState = &dynamic_state_info;
            graphic_pipeline_info.pViewportState = &viewport_state_info;
            graphic_pipeline_info.pDepthStencilState = &depth_state_info;
            graphic_pipeline_info.pStages = shader_stage_create_infos.data();
            graphic_pipeline_info.stageCount = shader_stage_create_infos.size();
            graphic_pipeline_info.renderPass = static_cast<RenderPassVulkan*>(render_pass)->render_pass;

            VkPipeline pipeline;
            VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, nullptr, 1, &graphic_pipeline_info, nullptr, &pipeline));
            if(desc.name.data() != nullptr)
            {
                const auto name = desc.name.data();
                set_label(reinterpret_cast<u64>(pipeline),VK_OBJECT_TYPE_PIPELINE,name);
                set_label(reinterpret_cast<u64>(pipeline_layout),VK_OBJECT_TYPE_PIPELINE_LAYOUT,name);
            }
            std::vector<VkDescriptorPoolSize> descriptor_pool_sizes;
            for (auto bindings : set_layout_info)
            {
                for (auto bd : bindings) {
                    auto it = std::find_if(descriptor_pool_sizes.begin(), descriptor_pool_sizes.end(), [bd](const VkDescriptorPoolSize& item) {
                        return bd.second == item.type;
                        });
                    if (it != descriptor_pool_sizes.end())
                        it->descriptorCount += 1;
                    else
                        descriptor_pool_sizes.push_back({
                        bd.second,
                        1
                            });
                }
            }
            auto raster_pipeline = std::make_shared<RasterPipelineVulkan>(pipeline_layout, pipeline, std::move(set_layout_info), std::move(descriptor_pool_sizes), std::move(descriptor_set_layouts), VK_PIPELINE_BIND_POINT_GRAPHICS);
            raster_pipeline->ty = GpuPipeline::PieplineType::Raster;
            return raster_pipeline;
        }

        auto GpuDeviceVulkan::create_ray_tracing_pipeline(const std::vector<PipelineShader>& shaders, const RayTracingPipelineDesc& desc) -> std::shared_ptr<RayTracingPipeline> 
        {
            std::vector<StageDescriptorSetLayouts> stage_layouts;
            for (auto shader : shaders) {
                stage_layouts.emplace_back(get_descriptor_sets(shader.code));
            }
            auto [descriptor_set_layouts, set_layout_info] = create_descriptor_set_layouts(*this, std::move(merge_shader_stage_layouts(stage_layouts)), VkShaderStageFlagBits::VK_SHADER_STAGE_ALL, desc.descriptor_set_opts);

            VkPipelineLayoutCreateInfo layout_create_info = {};
            layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            layout_create_info.pSetLayouts = descriptor_set_layouts.data();
            layout_create_info.setLayoutCount = (uint32_t)descriptor_set_layouts.size();
    
            VkPipelineLayout pipeline_layout;
            VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layout_create_info, nullptr, &pipeline_layout));

            std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
            std::vector<VkPipelineShaderStageCreateInfo>  shader_stages;

            std::vector<std::string> entry_points;
            u32 raygen_entry_count = 0;
            u32 miss_entry_count = 0;
            u32 hit_entry_count = 0;

            auto create_shader_module = [&](const PipelineShader& desc)->std::pair<VkShaderModule, const char*> {
                VkShaderModuleCreateInfo    shader_create_info = {};
                shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                shader_create_info.pCode = (uint32*)desc.code.codes.data();
                shader_create_info.codeSize = desc.code.codes.size();
                VkShaderModule shader_module;
                VK_CHECK_RESULT(vkCreateShaderModule(device, &shader_create_info, nullptr, &shader_module));
                
                return { shader_module ,desc.desc.source.entry.c_str()};
            };

            auto prev_stage = std::optional<ShaderPipelineStage>{};
            
            for (auto& desc : shaders)
            {
                auto group_idx = shader_stages.size();
                switch ( desc.desc.stage)
                {
                case ShaderPipelineStage::RayGen:
                {
                    assert(!prev_stage || *prev_stage == ShaderPipelineStage::RayGen);
                    raygen_entry_count += 1;
                    auto [shader_module, entry_point] = create_shader_module(desc);
                    //entry_points.push_back(entry_point);
                    
                    VkPipelineShaderStageCreateInfo stage = {};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.module = shader_module;
                    stage.pName = entry_point;
                    stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    VkRayTracingShaderGroupCreateInfoKHR group = {};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    group.generalShader = group_idx;
                    group.closestHitShader = VK_SHADER_UNUSED_KHR;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shader_stages.push_back(stage);
                    shader_groups.push_back(group);
                }break;
                case ShaderPipelineStage::RayClosestHit:
                {
                    assert(
                        *prev_stage == (ShaderPipelineStage::RayMiss)
                        || *prev_stage == (ShaderPipelineStage::RayClosestHit)
                        );
                    hit_entry_count += 1;

                    auto [shader_module, entry_point] = create_shader_module(desc);
                    //entry_points.push_back(entry_point);
                    set_label(reinterpret_cast<u64>(shader_module), VK_OBJECT_TYPE_SHADER_MODULE, desc.desc.source.path.c_str());
                    VkPipelineShaderStageCreateInfo stage = {};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.module = shader_module;
                    stage.pName = entry_point;
                    stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                    VkRayTracingShaderGroupCreateInfoKHR group = {};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                    group.generalShader = VK_SHADER_UNUSED_KHR;
                    group.closestHitShader = group_idx;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shader_stages.push_back(stage);
                    shader_groups.push_back(group);
                }break;
                case ShaderPipelineStage::RayMiss:
                {
                    assert(
                        *prev_stage == (ShaderPipelineStage::RayGen)
                        || *prev_stage == (ShaderPipelineStage::RayMiss)
                        );
                    miss_entry_count += 1;

                    auto [shader_module, entry_point] = create_shader_module(desc);
                    //entry_points.push_back(entry_point);
                    VkPipelineShaderStageCreateInfo stage = {};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.module = shader_module;
                    stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_MISS_BIT_KHR;
                    stage.pName = entry_point;

                    VkRayTracingShaderGroupCreateInfoKHR group = {};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    group.generalShader = group_idx;
                    group.closestHitShader = VK_SHADER_UNUSED_KHR;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shader_stages.push_back(stage);
                    shader_groups.push_back(group);

                }break;
                case ShaderPipelineStage::RayAnyHit:
                {
                    assert(
                        prev_stage == ShaderPipelineStage::RayAnyHit
                        || prev_stage == ShaderPipelineStage::RayClosestHit
                        || prev_stage == ShaderPipelineStage::RayMiss
                        );

                    auto [shader_module, entry_point] = create_shader_module(desc);
                    //entry_points.push_back(entry_point);
                    VkPipelineShaderStageCreateInfo stage = {};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.module = shader_module;
                    stage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
                    stage.pName = entry_point;

                    if (prev_stage == ShaderPipelineStage::RayClosestHit)
                    {
                        shader_groups.back().anyHitShader = shader_stages.size();
                    }
                    else
                    { 
                        VkRayTracingShaderGroupCreateInfoKHR group = {};
                        group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                        group.type = VkRayTracingShaderGroupTypeKHR::VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                        group.generalShader = VK_SHADER_UNUSED_KHR;
                        group.closestHitShader = VK_SHADER_UNUSED_KHR;
                        group.anyHitShader = shader_stages.size();
                        group.intersectionShader = VK_SHADER_UNUSED_KHR;
                        shader_groups.push_back(group);
                        hit_entry_count += 1;
                    }
                    shader_stages.push_back(stage);
                }break;
                default:
                    break;
                }

                prev_stage = desc.desc.stage;
            }
            assert(raygen_entry_count > 0);
            assert(miss_entry_count > 0);

            VkRayTracingPipelineCreateInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            info.flags = 0;
            info.pStages = shader_stages.data();
            info.stageCount = (uint32_t)shader_stages.size();
            info.groupCount = (uint32_t)shader_groups.size();
            info.pGroups = shader_groups.data();
            info.maxPipelineRayRecursionDepth = desc.max_pipeline_ray_recursion_depth;
            info.layout = pipeline_layout;

            VkPipeline pipeline;
            vkCreateRayTracingPipelinesKHR(device,
                VK_NULL_HANDLE,
                nullptr,
                1,
                &info,
                nullptr,
                &pipeline);
            if(desc.name.data() != nullptr)
            {
                const auto name = desc.name.data();
                set_label(reinterpret_cast<u64>(pipeline),VK_OBJECT_TYPE_PIPELINE,name);
                set_label(reinterpret_cast<u64>(pipeline_layout),VK_OBJECT_TYPE_PIPELINE_LAYOUT,name);
            }
            std::vector<VkDescriptorPoolSize> descriptor_pool_sizes;
            for (auto bindings : set_layout_info)
            {
                for (auto bd : bindings) {
                    auto it = std::find_if(descriptor_pool_sizes.begin(), descriptor_pool_sizes.end(), [bd](const VkDescriptorPoolSize& item) {
                        return bd.second == item.type;
                        });
                    if (it != descriptor_pool_sizes.end())
                        it->descriptorCount += 1;
                    else
                        descriptor_pool_sizes.push_back({
                            bd.second,
                            1
                        });
                }
            }
            
            auto stb = create_ray_tracing_shader_table(RayTracingShaderTableDesc{
                   raygen_entry_count,
                    hit_entry_count,
                    miss_entry_count
                }, pipeline);

            auto rt_pipeline = std::make_shared<RayTracingPipelineVulkan>(
                pipeline_layout, 
                pipeline, 
                std::move(set_layout_info), 
                std::move(descriptor_pool_sizes), 
                std::move(descriptor_set_layouts),
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
                stb);
            rt_pipeline->group_size = {u32(0), u32(0) ,u32(0)};
            rt_pipeline->ty = GpuPipeline::PieplineType::RayTracing;
            return rt_pipeline;
        }

        void GpuDeviceVulkan::immediate_destroy_buffer(GpuBufferVulkan& buffer)
        {
            if( buffer.handle != VK_NULL_HANDLE)
            { 
                cb_mutex.lock();
                vmaDestroyBuffer(global_allocator, buffer.handle, buffer.allocation);
                buffer.handle = VK_NULL_HANDLE;
                buffer.allocation = nullptr;
                cb_mutex.unlock();
            }
        }

        void GpuDeviceVulkan::setup_cmd_buffer(std::function<void(VkCommandBuffer cmd)> callback)
        {
            std::lock_guard<std::mutex>	lock(cb_mutex);

            VkCommandBufferBeginInfo	begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(setup_cb->handle, &begin_info);
            callback(setup_cb->handle);
            vkEndCommandBuffer(setup_cb->handle);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = VK_NULL_HANDLE;
            submitInfo.pCommandBuffers = &setup_cb->handle;
            submitInfo.commandBufferCount = 1;
            vkResetFences(device, 1, &setup_cb->submit_done_fence);
            vkQueueSubmit(setup_cb->queue, 1, &submitInfo, setup_cb->submit_done_fence);

            vkQueueWaitIdle(setup_cb->queue);
            // vkDeviceWaitIdle(device);
        }

        void GpuDeviceVulkan::setup_graphics_queue_cmd_buffer(std::function<void(VkCommandBuffer cmd)> callback)
        {
            std::lock_guard<std::mutex>	lock(cb_mutex);

            VkCommandBufferBeginInfo	begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(graphics_queue_setup_cb->handle, &begin_info);
            callback(graphics_queue_setup_cb->handle);
            vkEndCommandBuffer(graphics_queue_setup_cb->handle);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = VK_NULL_HANDLE;
            submitInfo.pCommandBuffers = &graphics_queue_setup_cb->handle;
            submitInfo.commandBufferCount = 1;
            vkResetFences(device, 1, &graphics_queue_setup_cb->submit_done_fence);
            vkQueueSubmit(graphics_queue_setup_cb->queue, 1, &submitInfo, graphics_queue_setup_cb->submit_done_fence);

            // vkQueueWaitIdle(graphics_queue_setup_cb->queue);
            // vkDeviceWaitIdle(device);
            vkWaitForFences(device, 1, &graphics_queue_setup_cb->submit_done_fence, VK_TRUE, UINT64_MAX);
        }

        std::shared_ptr<GpuBufferVulkan> GpuDeviceVulkan::create_buffer_impl(VkDevice device, VmaAllocator& allocator,const GpuBufferDesc& desc, const char* name)
        {
            const u32 alignment = gpu_limits.minImportedHostPointerAlignment;
            VkBufferCreateInfo	buffer_create_info = {};
            buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_create_info.size = desc.size;//(desc.size + alignment - 1) &~ (alignment - 1);
            buffer_create_info.usage = (VkBufferUsageFlagBits)(desc.usage);
            buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            
            VkBuffer	buffer;
            VmaAllocation	allocation;
            VmaAllocationCreateInfo vbAllocCreateInfo = {};
            VmaAllocationInfo allocationInfo{};
#if 0
            vbAllocCreateInfo.usage = (VmaMemoryUsage)desc.memory_usage;
            VK_CHECK_RESULT(vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer));
            VkMemoryRequirements	mem_requirements;
            vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

            if (desc.usage & BufferUsageFlags::SHADER_BINDING_TABLE)
            {
                mem_requirements.alignment = std::max<uint64>(mem_requirements.alignment, 64);
            }
            cb_mutex.lock();
            VK_CHECK_RESULT(vmaAllocateMemory(allocator, &mem_requirements, &vbAllocCreateInfo, &allocation, nullptr));
            VK_CHECK_RESULT(vkBindBufferMemory(device, buffer, allocation->GetMemory(), allocation->GetOffset()));
            cb_mutex.unlock();
#else
 /*           if(buffer_create_info.size <= SMALL_ALLOCATION_MAX_SIZE)
            {
                uint32_t mem_type_index = 0;
                vmaFindMemoryTypeIndexForBufferInfo(global_allocator, &buffer_create_info, &vbAllocCreateInfo, &mem_type_index);
                vbAllocCreateInfo.pool = get_or_create_small_alloc_pool(mem_type_index);
            }*/
            uint32_t memoryTypeIndex = 0;
            auto desiredMemoryPropertyFlags = desc.memory_usage != MemoryUsage::GPU_ONLY ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0;
            for (uint32_t i = 0; i < physcial_device.mem_properties.memoryTypeCount; i++) {
                // Check if this memory type belongs to a suitable heap
                auto heapIndex = physcial_device.mem_properties.memoryTypes[i].heapIndex;
                if((physcial_device.mem_properties.memoryTypes[i].propertyFlags & desiredMemoryPropertyFlags) == desiredMemoryPropertyFlags){
                    memoryTypeIndex = i;
                    break;
                }
            }
            vbAllocCreateInfo.memoryTypeBits = 1 << memoryTypeIndex;
            vbAllocCreateInfo.usage = (VmaMemoryUsage)desc.memory_usage;
            vbAllocCreateInfo.flags = desc.memory_usage != MemoryUsage::GPU_ONLY ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;
            vbAllocCreateInfo.preferredFlags = desiredMemoryPropertyFlags;
            cb_mutex.lock();
            VK_CHECK_RESULT(vmaCreateBuffer(allocator, &buffer_create_info, &vbAllocCreateInfo, &buffer, &allocation, &allocationInfo));
            cb_mutex.unlock();
#endif
            return std::make_shared<GpuBufferVulkan>(buffer, desc, allocation, allocationInfo);
        }

        auto GpuDeviceVulkan::create_ray_tracing_shader_table(const RayTracingShaderTableDesc& desc, VkPipeline pipeline) -> RayTracingShaderTable
        {
        #ifndef DS_PRODUCTION
            DS_LOG_INFO("Creating ray tracing shader table: {}", desc);
        #endif
            auto shader_group_handle_size = raytracingProperties.shaderGroupHandleSize;

            auto group_count = u32(desc.raygen_entry_count + desc.miss_entry_count + desc.hit_entry_count);
            auto group_handles_size = shader_group_handle_size * group_count;
            std::vector<u8> group_handles(group_handles_size);
            vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, group_count, group_handles_size, group_handles.data());
            auto prog_size = shader_group_handle_size;

            auto create_binding_table = [&](u32 entry_offset,u32 entry_count)->std::shared_ptr<GpuBuffer> {
                
                if( entry_count == 0) return nullptr;
                std::vector<u8> shader_binding_table_data(entry_count * prog_size, 0);
              
                for (auto dst = 0; dst < entry_count; dst++) {
                    auto src = dst + entry_offset;
                    std::memcpy(shader_binding_table_data.data() + dst * prog_size, 
                        group_handles.data() + src * shader_group_handle_size, shader_group_handle_size);
                }
                auto buffer_desc = GpuBufferDesc::new_gpu_only(shader_binding_table_data.size(), 
                                    BufferUsageFlags::TRANSFER_SRC |
                                    BufferUsageFlags::SHADER_DEVICE_ADDRESS |
                                    BufferUsageFlags::SHADER_BINDING_TABLE);
                return create_buffer(buffer_desc,
                            "STB sub-buffer",
                            shader_binding_table_data.data()
                            );
            };

            auto raygen_shader_binding_table = create_binding_table(0, desc.raygen_entry_count);
            auto miss_shader_binding_table = create_binding_table(desc.raygen_entry_count, desc.miss_entry_count);
            auto hit_shader_binding_table = create_binding_table( desc.raygen_entry_count + desc.miss_entry_count, desc.hit_entry_count);


            return RayTracingShaderTable{
                raygen_shader_binding_table,
                VkStridedDeviceAddressRegionKHR{
                    raygen_shader_binding_table->device_address(this),
                    prog_size,
                    prog_size * desc.raygen_entry_count
                },
                 miss_shader_binding_table,
                VkStridedDeviceAddressRegionKHR{
                    miss_shader_binding_table->device_address(this),
                    prog_size,
                    prog_size * desc.miss_entry_count
                },

                hit_shader_binding_table,
                VkStridedDeviceAddressRegionKHR{
                    hit_shader_binding_table->device_address(this),
                    prog_size,
                    prog_size * desc.hit_entry_count
                },
                nullptr,
                VkStridedDeviceAddressRegionKHR{
                    0,
                    0,
                    0
                }
            };
        }

        auto GpuDeviceVulkan::create_ray_tracing_acceleration(VkAccelerationStructureTypeKHR ty,
            VkAccelerationStructureBuildGeometryInfoKHR& geometry_info,
            const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& build_range_infos,
            const std::vector<u32>& primitive_counts,
            u64 preallocate_bytes,
            const std::optional<RayTracingAccelerationScratchBuffer>& scratch_buffer_opt)->std::shared_ptr<GpuRayTracingAcceleration>
        {
            VkAccelerationStructureBuildSizesInfoKHR memory_requirements{};
            memory_requirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(
                device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometry_info,
                primitive_counts.data(),
                &memory_requirements);
        #ifndef DS_PRODUCTION
            DS_LOG_INFO("Acceleration structure size: {}, scratch size: {}", memory_requirements.accelerationStructureSize, memory_requirements.buildScratchSize);
        #endif
            auto backing_buffer_size = std::max(preallocate_bytes, memory_requirements.accelerationStructureSize);

            auto buffer_desc = GpuBufferDesc::new_gpu_only(backing_buffer_size, BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR |
                                    BufferUsageFlags::SHADER_DEVICE_ADDRESS);
            auto accel_buffer = create_buffer(buffer_desc, "Acceleration structure buffer", nullptr);
            VkAccelerationStructureCreateInfoKHR accel_info = {};
            accel_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accel_info.buffer = reinterpret_cast<GpuBufferVulkan*>(accel_buffer.get())->handle;
            accel_info.size = backing_buffer_size;
            accel_info.type = ty;

            std::optional<std::shared_ptr<rhi::GpuBuffer>>  tmp_scratch_buffer;
            std::shared_ptr<rhi::GpuBuffer> scratch_buffer;
            if (scratch_buffer_opt)
            {
                scratch_buffer = scratch_buffer_opt->buffer;
            }
            else
            {  
                auto tmp_buffer_desc = GpuBufferDesc::new_gpu_only(memory_requirements.buildScratchSize, BufferUsageFlags::STORAGE_BUFFER | BufferUsageFlags::SHADER_DEVICE_ADDRESS).alignment(256);
                
                scratch_buffer = create_buffer(tmp_buffer_desc, "Acceleration structure scratch buffer", nullptr);
            }

            VkAccelerationStructureKHR accel;
            vkCreateAccelerationStructureKHR(device, &accel_info, nullptr, &accel);
            assert(memory_requirements.buildScratchSize <= scratch_buffer->desc.size);

            geometry_info.dstAccelerationStructure = accel;
            geometry_info.scratchData = VkDeviceOrHostAddressKHR{ scratch_buffer->device_address(this)};

            auto pRangeInfo = build_range_infos.data();

            this->setup_graphics_queue_cmd_buffer([&](VkCommandBuffer cmd) {
                vkCmdBuildAccelerationStructuresKHR(cmd, 1, &geometry_info, &pRangeInfo);

                VkMemoryBarrier memory_barrier = {};
                memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

                vkCmdPipelineBarrier(cmd, 
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    0,
                    1,
                    &memory_barrier,
                    0,
                    nullptr,
                    0,
                    nullptr);
            });
     
            if(tmp_scratch_buffer)
            { 
                auto buffer = reinterpret_cast<GpuBufferVulkan*>(tmp_scratch_buffer->get());
                immediate_destroy_buffer(*buffer);
            }

            return std::make_shared<GpuRayTracingAccelerationVulkan>(accel, accel_buffer);
        }

        auto GpuDeviceVulkan::create_ray_tracing_bottom_acceleration(const RayTracingBottomAccelerationDesc& desc) -> std::shared_ptr<GpuRayTracingAcceleration>
        {
            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            for (auto& geo_desc : desc.geometries)
            {
                auto& parts = geo_desc.parts;
                for(auto& part : parts)
                { 
                    VkAccelerationStructureGeometryKHR geometry = {};
                    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    geometry.flags = /*true ? VK_GEOMETRY_OPAQUE_BIT_KHR :*/ VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
                    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                    geometry.geometry.triangles.vertexFormat = pixel_format_2_vk(geo_desc.vertex_format);
                    geometry.geometry.triangles.vertexData.deviceAddress = part.vertex_buffer_address;
                    geometry.geometry.triangles.maxVertex = part.max_vertex;
                    geometry.geometry.triangles.vertexStride = geo_desc.vertex_stride;
                    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                    geometry.geometry.triangles.indexData.deviceAddress = part.index_buffer_address;
                    geometry.geometry.triangles.transformData.deviceAddress = 0;
                    geometry.geometry.triangles.transformData.hostAddress = nullptr;

                    geometries.push_back(geometry);            
                }
            }
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> build_range_infos = {};
            for (auto& geo_desc : desc.geometries)
            {
                for (auto& part : geo_desc.parts)
                {
                    VkAccelerationStructureBuildRangeInfoKHR build_info = {};
                    build_info.primitiveCount = part.index_count / 3;

                    build_range_infos.push_back(build_info);
                }
            }
            // Get size info
            VkAccelerationStructureBuildGeometryInfoKHR geometry_info{};
            geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            geometry_info.geometryCount = geometries.size();
            geometry_info.pGeometries = geometries.data();
            //if (desc.flags & BottomLevelDesc::FLAG_ALLOW_UPDATE)
            //{
            //    geometry_info.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
            //}
            //if (desc.flags & BottomLevelDesc::FLAG_ALLOW_COMPACTION)
            //{
            //    geometry_info.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            //}
            //if (desc.flags & BottomLevelDesc::FLAG_PREFER_FAST_TRACE)
            //{
            //    geometry_info.flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            //}
            //if (desc.flags & BottomLevelDesc::FLAG_PREFER_FAST_BUILD)
            //{
            //    geometry_info.flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
            //}
            //if (desc.flags & BottomLevelDesc::FLAG_MINIMIZE_MEMORY)
            //{
            //    geometry_info.flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
            //}

            std::vector<u32> max_primitive_counts;
            for (auto& gemo_desc : desc.geometries)
            { 
                for (auto& part : gemo_desc.parts)
                    max_primitive_counts.push_back(part.index_count / 3);
            }
            u64 preallocate_bytes = 0;
            return create_ray_tracing_acceleration(
                VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, 
                geometry_info,
                build_range_infos,
                max_primitive_counts,
                preallocate_bytes,
                {});
        }

        auto GpuDeviceVulkan::create_ray_tracing_top_acceleration(const RayTracingTopAccelerationDesc& desc, const RayTracingAccelerationScratchBuffer& scratch_buffer) -> std::shared_ptr<GpuRayTracingAcceleration>
        {
            std::vector<GeometryInstance>   instances;

            for (auto& instance_desc : desc.instances)
            {
                VkAccelerationStructureDeviceAddressInfoKHR device_info = {};
                device_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                device_info.accelerationStructure = static_cast<GpuRayTracingAccelerationVulkan*>(instance_desc.blas.get())->acc;
                auto blas_address = vkGetAccelerationStructureDeviceAddressKHR(device, &device_info);

                //instance: todo
                auto transform = instance_desc.transformation;

                instances.push_back(GeometryInstance(transform, instance_desc.mesh_index, instance_desc.mask, 0, GeometryInstanceFlag::TRIANGLE_FACING_CULL_DISABLE, blas_address));
            }
            auto instance_buffer_size = sizeof(GeometryInstance) * std::max<size_t>(1, instances.size());
            auto buffer_desc = GpuBufferDesc::new_gpu_only(
                instance_buffer_size,
                BufferUsageFlags::SHADER_DEVICE_ADDRESS | 
                //BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR |
                BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY
            );
            auto inst_data = (u8*)instances.data();
            auto instance_buffer = create_buffer(buffer_desc, "TLAS instance buffer", inst_data);
            auto instance_buffer_address = instance_buffer->device_address(this);

            VkAccelerationStructureGeometryKHR geometry = {};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_INSTANCES_KHR;
            geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            geometry.geometry.instances.data.deviceAddress = instance_buffer_address;

            std::vector<VkAccelerationStructureBuildRangeInfoKHR>    build_range_infos = {};
            VkAccelerationStructureBuildRangeInfoKHR build_range_info = {};
            build_range_info.primitiveCount = (u64)instances.size();
            build_range_infos.push_back(build_range_info);

            VkAccelerationStructureBuildGeometryInfoKHR geometry_info = {};
            geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            geometry_info.geometryCount = 1;
            geometry_info.pGeometries = &geometry;

            std::vector<u32> max_primitive_counts = {(u32)instances.size()};

            return create_ray_tracing_acceleration(
                VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                geometry_info,
                build_range_infos,
                max_primitive_counts,
                desc.preallocate_bytes,
                scratch_buffer
            );
        }

        auto GpuDeviceVulkan::rebuild_ray_tracing_top_acceleration(CommandBuffer* cb, u64 instance_buffer_address, u64 instance_count, GpuRayTracingAcceleration* tlas, RayTracingAccelerationScratchBuffer* scratch_buffer) -> void
        {
            VkAccelerationStructureGeometryKHR geometry = {};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_INSTANCES_KHR;
            geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            geometry.geometry.instances.data.deviceAddress = instance_buffer_address;

            std::vector<VkAccelerationStructureBuildRangeInfoKHR>    build_range_infos = {};
            VkAccelerationStructureBuildRangeInfoKHR build_range_info = {};
            build_range_info.primitiveCount = instance_count;
            build_range_infos.push_back(build_range_info);

            VkAccelerationStructureBuildGeometryInfoKHR geometry_info = {};
            geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            geometry_info.geometryCount = 1;
            geometry_info.pGeometries = &geometry;

            std::vector<u32> max_primitive_counts = { (u32)instance_count };

            auto vk_cb = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            rebuild_ray_tracing_acceleration(
                vk_cb,
                geometry_info,
                build_range_infos,
                max_primitive_counts,
                static_cast<GpuRayTracingAccelerationVulkan*>(tlas),
                scratch_buffer);
        }

        auto GpuDeviceVulkan::with_setup_cb(std::function<void(CommandBuffer* cmd)>&& callback) -> void
        {
            std::lock_guard<std::mutex>	lock(cb_mutex);

            VkCommandBufferBeginInfo	begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(setup_cb->handle, &begin_info);
            callback(setup_cb.get());
            vkEndCommandBuffer(setup_cb->handle);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext = VK_NULL_HANDLE;
            submitInfo.pCommandBuffers = &setup_cb->handle;
            submitInfo.commandBufferCount = 1;
            vkResetFences(device, 1, &setup_cb->submit_done_fence);
            vkQueueSubmit(setup_cb->queue, 1, &submitInfo, setup_cb->submit_done_fence);

            vkQueueWaitIdle(setup_cb->queue);
            //vkDeviceWaitIdle(device);
        }

        auto GpuDeviceVulkan::copy_buffer(CommandBuffer* cmd, GpuBuffer* src, u64 src_offset, GpuBuffer* dst, u64 dst_offset, u64 size_) -> void
        {
            auto vk_cb = static_cast<GpuCommandBufferVulkan*>(cmd)->handle;
            auto src_buf = static_cast<GpuBufferVulkan*>(src)->handle;
            auto dst_buf = static_cast<GpuBufferVulkan*>(dst)->handle;

            VkBufferCopy    region = {};
            region.srcOffset = src_offset;
            region.dstOffset = dst_offset;
            region.size = size_;
            vkCmdCopyBuffer(vk_cb, src_buf, dst_buf, 1,  &region);
        }

        auto GpuDeviceVulkan::rebuild_ray_tracing_acceleration(VkCommandBuffer cmd,
            VkAccelerationStructureBuildGeometryInfoKHR& geometry_info,
            const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& build_range_infos,
            const std::vector<u32>& max_primitive_counts,
            GpuRayTracingAccelerationVulkan* accel,
            RayTracingAccelerationScratchBuffer* scratch_buffer) -> void
        {
            VkAccelerationStructureBuildSizesInfoKHR memory_requirements{};
            memory_requirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(
                device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometry_info,
                max_primitive_counts.data(),
                &memory_requirements);

            assert(memory_requirements.accelerationStructureSize <= accel->backing_buffer->desc.size);
            auto scra_buf = scratch_buffer->buffer;
            assert(memory_requirements.buildScratchSize <= scra_buf->desc.size);

            geometry_info.dstAccelerationStructure = accel->acc;
            geometry_info.scratchData = VkDeviceOrHostAddressKHR{ scra_buf->device_address(this) };
            auto pRangeInfo = build_range_infos.data();

            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &geometry_info, &pRangeInfo);

            VkMemoryBarrier memory_barrier = {};
            memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

            vkCmdPipelineBarrier(cmd,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0,
                1,
                &memory_barrier,
                0,
                nullptr,
                0,
                nullptr);
        }

        auto GpuDeviceVulkan::defer_release(VkDescriptorPool pool)->void
        {
            cb_mutex.lock();
            frames[0].pending_resource_releases.descriptor_pools.push_back(pool);
            cb_mutex.unlock();
        }

        VmaPool GpuDeviceVulkan::get_or_create_small_alloc_pool(uint32_t memTypeIndex)
        {
            if (small_alloc_pools.find(memTypeIndex) != small_alloc_pools.end())
                return small_alloc_pools[memTypeIndex];

            DS_LOG_INFO("Creating VMA small objects pool for memory type index {0}", memTypeIndex);

            VmaPoolCreateInfo pci;
            pci.memoryTypeIndex = memTypeIndex;
            pci.flags = 0;
            pci.blockSize = 0;
            pci.minBlockCount = 0;
            pci.maxBlockCount = SIZE_MAX;
            pci.priority = 0.5f;
            pci.minAllocationAlignment = 0;
            pci.pMemoryAllocateNext = nullptr;
            VmaPool pool = VK_NULL_HANDLE;
            VK_CHECK_RESULT(vmaCreatePool(global_allocator, &pci, &pool));
            small_alloc_pools[memTypeIndex] = pool;
            return pool;
        }

        std::vector<PhysicalDevice> enumerate_physical_devices(const GpuInstanceVulkan& instance) 
        {
            std::vector<PhysicalDevice> pdevices;

            uint32 physical_device_count;

            vkEnumeratePhysicalDevices(instance.instance, &physical_device_count, nullptr);
            std::vector<VkPhysicalDevice>	vk_devices;
            vk_devices.resize(physical_device_count);
            vkEnumeratePhysicalDevices(instance.instance, &physical_device_count, vk_devices.data());

            
            for (auto vk_dev : vk_devices) {
                VkPhysicalDeviceProperties	properties;
                vkGetPhysicalDeviceProperties(vk_dev, &properties);

                uint32 fam_count;
                vkGetPhysicalDeviceQueueFamilyProperties(vk_dev,&fam_count, nullptr );
                std::vector<VkQueueFamilyProperties>	queue_properties;
                queue_properties.resize(fam_count);
                vkGetPhysicalDeviceQueueFamilyProperties(vk_dev, &fam_count, queue_properties.data());

                std::vector<QueueFamily> queue_familes;
                for (uint32 i = 0; i < fam_count; i++) {
                    queue_familes.push_back({i, queue_properties[i]});
                }
                VkPhysicalDeviceMemoryProperties memory_properties;
                vkGetPhysicalDeviceMemoryProperties(vk_dev, &memory_properties);

                pdevices.push_back({ instance, vk_dev, queue_familes, true, properties, memory_properties });
            }

            return pdevices;
        }

        std::vector<PhysicalDevice> presentation_support(const std::vector<PhysicalDevice>& physdevices, const SurfaceVulkan& surface)
        {
            std::vector<PhysicalDevice> pdevices;
            for (auto pdevice : physdevices) {
                pdevice.presentation_requested = true;
                for (auto queue_fam_index = 0; queue_fam_index < pdevice.queue_family.size(); queue_fam_index++) {
                    auto queue_fam = pdevice.queue_family[queue_fam_index];
                    if (queue_fam.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        VkBool32 bSupported = VK_FALSE;
                        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice.handle, queue_fam_index, surface.surface, &bSupported);
                        if (bSupported) {
                            pdevices.push_back(pdevice);
                            break;
                        }
                    }
                }
            }
            return pdevices;
        }
	
        auto GpuDeviceVulkan::begin_frame()->DeviceFrame*
        {
            std::lock_guard<std::mutex>	lock(frame_mutex[0]);
            auto& frame0 = frames[0];
            VkFence	fences[2] = { dynamic_pointer_cast<GpuCommandBufferVulkan>(frame0.main_cmd_buf)->submit_done_fence , 
                                dynamic_pointer_cast<GpuCommandBufferVulkan>(frame0.presentation_cmd_buf)->submit_done_fence};
            vkWaitForFences(device, 2, fences, true, UINT64_MAX);
    
            frame0.pending_resource_releases.release_all(device);
           // if( swapchain->current_frame_index() % 12 == 0)
            release_resources();
            for (auto& res : destroy_queue)
                res.frame_counter++;
            return &frame0;
        }

        void GpuDeviceVulkan::end_frame(DeviceFrame* frameRes)
        {
            std::lock_guard<std::mutex>	lock0(frame_mutex[0]);
            auto& frame0 = frames[0];
            auto& frame1 = frames[1];
            std::swap(frame0, frame1);
        }

        auto GpuDeviceVulkan::begin_cmd(CommandBuffer* cb)->void
        {
            cb->begin();
        }

        auto GpuDeviceVulkan::end_cmd(CommandBuffer* cb)->void
        {
            cb->end();
        }

        auto GpuDeviceVulkan::submit_cmd(CommandBuffer* cb)->void
        {
            auto cb_vk = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pCommandBuffers = &cb_vk->handle;
            submit_info.commandBufferCount = 1;

            vkResetFences(device, 1, &cb_vk->submit_done_fence);
            vkQueueSubmit(universe_queue.queue, 1, &submit_info, cb_vk->submit_done_fence);
        }

        auto GpuDeviceVulkan::execute_cmd(CommandBuffer* cb)->void
        {
            auto cb_vk = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            VkSubmitInfo submit_info = {};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.pCommandBuffers = &cb_vk->handle;
            submit_info.commandBufferCount = 1;

            vkResetFences(device, 1, &cb_vk->submit_done_fence);
            vkQueueSubmit(universe_queue.queue, 1, &submit_info, cb_vk->submit_done_fence);
            VkFence	fences[1] = { cb_vk->submit_done_fence};
            vkWaitForFences(device, 1, fences, true, UINT64_MAX);
        }
        
        auto GpuDeviceVulkan::record_image_barrier(CommandBuffer* cb, const ImageBarrier& barrier) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            int queue_family_index = universe_queue.family.index;
            if (vk_cb->family_index == compute_queue.family.index) {
                queue_family_index = compute_queue.family.index;
            }
            else if (vk_cb->family_index == transfer_queue.family.index) {
                queue_family_index = transfer_queue.family.index;
            }
            rhi::record_image_barrier(*this, vk_cb->handle, barrier, queue_family_index, queue_family_index);
        }

        auto GpuDeviceVulkan::record_buffer_barrier(CommandBuffer* cb, const BufferBarrier& barrier) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            int queue_family_index = universe_queue.family.index;
            if (vk_cb->family_index == compute_queue.family.index) {
                queue_family_index = compute_queue.family.index;
            }
            else if (vk_cb->family_index == transfer_queue.family.index) {
                queue_family_index = transfer_queue.family.index;
            }
            vk::pipeline_barrier(device, 
                vk_cb->handle,
                std::optional<vk::GlobalBarrier>(), 
                {vk::BufferBarrier{
                {barrier.prev_access},
				{barrier.next_access},
                (u32)queue_family_index,
                (u32)queue_family_index,
                static_cast<rhi::GpuBufferVulkan*>(barrier.buffer)->handle,
                barrier.offset,
                barrier.size
            }}, {} );
        }

        auto GpuDeviceVulkan::record_global_barrier(CommandBuffer* cb, const std::vector<rhi::AccessType>& previous_accesses, const std::vector<rhi::AccessType>& next_accesses) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            vk::GlobalBarrier gb_barrier = { previous_accesses,  next_accesses };
            vk::pipeline_barrier(device, vk_cb->handle, gb_barrier, {}, {});
        }

        auto GpuCommandBufferVulkan::begin() -> void
        {
            vkResetCommandBuffer(handle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(handle, &begin_info);
        }
        auto GpuCommandBufferVulkan::end() -> void
        {
            vkEndCommandBuffer(handle);
        }

        auto GpuDeviceVulkan::dispatch(CommandBuffer* cb,const std::array<u32, 3>& group_dim,const std::array<u32,3>& group_size) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            vkCmdDispatch(vk_cb->handle,group_dim[0], group_dim[1], group_dim[2]);
        }

        auto GpuDeviceVulkan::dispatch_indirect(CommandBuffer* cb,GpuBuffer* args_buffer, u64 args_buffer_offset) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            auto vk_buffer = dynamic_cast<GpuBufferVulkan*>(args_buffer);
            vkCmdDispatchIndirect(vk_cb->handle, vk_buffer->handle, args_buffer_offset);
        }

        auto GpuDeviceVulkan::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, rhi::GpuBuffer* buffer,u32 array_index)->void
        {
            VkDescriptorBufferInfo  buffer_info = {};
            buffer_info.buffer = reinterpret_cast<GpuBufferVulkan*>(buffer)->handle;
            buffer_info.offset = 0;
            buffer_info.range = std::numeric_limits<u64>::max();

            VkWriteDescriptorSet    write_descriptor_set = {};
            write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_descriptor_set.dstSet = reinterpret_cast<VulkanDescriptorSet*>(descriptor_set)->descriptor_set;
            write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write_descriptor_set.dstBinding = dst_binding;
            write_descriptor_set.pBufferInfo = &buffer_info;
            write_descriptor_set.descriptorCount = 1;
            write_descriptor_set.dstArrayElement = array_index;

            vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
        }

        auto GpuDeviceVulkan::write_descriptor_set(DescriptorSet* descriptor_set, u32 dst_binding, u32 array_index,const rhi::DescriptorImageInfo& img_info) -> void
        {
            VkDescriptorImageInfo  image_info = {};
            image_info.imageLayout = (VkImageLayout)img_info.image_layout;
            image_info.imageView = reinterpret_cast<GpuTextureViewVulkan*>(img_info.view)->img_view;

            VkWriteDescriptorSet    write_descriptor_set = {};
            write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_descriptor_set.dstSet = reinterpret_cast<VulkanDescriptorSet*>(descriptor_set)->descriptor_set;
            write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write_descriptor_set.dstBinding = dst_binding;
            write_descriptor_set.pImageInfo = &image_info;
            write_descriptor_set.dstArrayElement = array_index;
            write_descriptor_set.descriptorCount = 1;

            vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
        }

        auto GpuDeviceVulkan::clear_depth_stencil(CommandBuffer* cb, GpuTexture* texture, float depth, u32 stencil) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            auto vk_img = dynamic_cast<GpuTextureVulkan*>(texture);
            VkClearDepthStencilValue depthStencil = { depth,stencil};
            VkImageSubresourceRange  sub_data = {};
            sub_data.levelCount = 1;
            sub_data.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            sub_data.layerCount = 1;

            vkCmdClearDepthStencilImage(vk_cb->handle, vk_img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencil, 1,  &sub_data);
        }

        auto GpuDeviceVulkan::clear_color(CommandBuffer* cb, GpuTexture* texture, const std::array<f32, 4>& color) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            auto vk_img = dynamic_cast<GpuTextureVulkan*>(texture);
            VkClearColorValue color_value = { color[0], color[1], color[2], color[3]};
            VkImageSubresourceRange  sub_data = {};
            sub_data.levelCount = texture->desc.mip_levels;
            sub_data.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            sub_data.layerCount =  texture->desc.array_elements;
            vkCmdClearColorImage(vk_cb->handle, vk_img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color_value,1, &sub_data);
        }

        auto GpuDeviceVulkan::set_point_size(CommandBuffer* cb, float point_size) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            vkCmdSetLineWidth(vk_cb->handle, point_size);
        }

        auto GpuDeviceVulkan::set_line_width(CommandBuffer* cb, float line_width) -> void 
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb);
            vkCmdSetLineWidth(vk_cb->handle, line_width);
        }
        auto GpuDeviceVulkan::set_viewport(CommandBuffer* cb, const ViewPort& view, const Scissor& scissors) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
            VkViewport p = {
                view.x,view.y,view.width, view.height, view.min_depth,view.max_depth
            };
            VkRect2D    rc = {
                {scissors.offset[0],scissors.offset[1]},
                {scissors.extent[0],scissors.extent[1]}
            };
            vkCmdSetViewport(vk_cb, 0, 1, &p);
            vkCmdSetScissor(vk_cb, 0, 1, &rc);
        }
        auto GpuDeviceVulkan::begin_render_pass(CommandBuffer* cb, 
                    const std::array<u32,2>& dims,
                    RenderPass* render_pass,
                    const std::vector<rhi::GpuTexture*>& color_tex,
                    rhi::GpuTexture* depth) -> void
        {
           auto vk_pass = dynamic_cast<RenderPassVulkan*>(render_pass);
           auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
           auto frame_buffer = vk_pass->framebuffer_cache.get_or_create(*this, {
                FramebufferCacheKey(dims, color_tex, depth)
            });
            std::vector<VkImageView> image_attachments;
            for (const auto& tex : color_tex)
            {
                auto view =  tex->view(this, rhi::GpuTextureViewDesc());
                auto vk_view = reinterpret_cast<GpuTextureViewVulkan*>(view.get());
                image_attachments.push_back(vk_view->img_view);
            }
            auto vk_view = reinterpret_cast<GpuTextureViewVulkan*>(depth->view(this, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH)).get());
            image_attachments.push_back(vk_view->img_view);

            VkRenderPassAttachmentBeginInfoKHR  pass_attachment_desc = {};
            pass_attachment_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR;
            pass_attachment_desc.attachmentCount = image_attachments.size();
            pass_attachment_desc.pAttachments = image_attachments.data();

            auto [width, height] = dims;

            auto clear_values = VkClearValue{ VkClearColorValue {{0.0f,0.0f,0.0f,0.0f}}};
  
            VkRenderPassBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            begin_info.framebuffer = frame_buffer;
            begin_info.renderArea = {
                {0,0},
                {width,height}
            };
            begin_info.renderPass = vk_pass->render_pass;
            begin_info.clearValueCount = 4;
            begin_info.pClearValues = &clear_values;
            begin_info.pNext = &pass_attachment_desc;
            
            vkCmdBeginRenderPass(vk_cb, &begin_info, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
        }

        auto GpuDeviceVulkan::end_render_pass(CommandBuffer* cb) -> void
        {
            auto vk_cb = dynamic_cast<GpuCommandBufferVulkan*>(cb)->handle;
           vkCmdEndRenderPass(vk_cb);
        }

        auto GpuDeviceVulkan::bind_vertex_buffers(CommandBuffer* cb, const GpuBuffer* const* vertexBuffers, uint32_t slot, uint32_t count, const uint32_t* strides, const uint64_t* offsets) -> void
        {
            VkDeviceSize voffsets[8] = {};
            VkDeviceSize vstrides[8] = {};
            VkBuffer vbuffers[8] = {};
            assert(count <= 8);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (vertexBuffers[i] == nullptr)
                {
                    vbuffers[i] = 0;
                }
                else
                {
                    auto internal_state = static_cast<const GpuBufferVulkan*>(vertexBuffers[i]);
                    vbuffers[i] = internal_state->handle;
                    if (offsets != nullptr)
                    {
                        voffsets[i] = offsets[i];
                    }
                    if (strides != nullptr)
                    {
                        vstrides[i] = strides[i];
                    }
                }
            }
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdBindVertexBuffers(cmd, 0, count, vbuffers, voffsets);
            //vkCmdBindVertexBuffers2(cmd, slot, count, vbuffers, voffsets, nullptr, vstrides);
            //vkCmdBindVertexBuffers2EXT(cmd, slot, count, vbuffers, voffsets, nullptr, vstrides);
        }
        auto GpuDeviceVulkan::bind_index_buffer(CommandBuffer* cb, const GpuBuffer* indexBuffer, const IndexBufferFormat format, uint64_t offset) -> void
        {
            if (indexBuffer != nullptr)
            {
                auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
                auto internal_state = static_cast<const GpuBufferVulkan*>(indexBuffer);
                vkCmdBindIndexBuffer(cmd, internal_state->handle, offset, format == IndexBufferFormat::UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            }
        }
        auto GpuDeviceVulkan::draw(CommandBuffer* cb, uint32_t vertexCount, uint32_t startVertexLocation) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdDraw(cmd, vertexCount, 1, startVertexLocation, 0);
        }
        auto GpuDeviceVulkan::draw_indexed(CommandBuffer* cb, uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdDrawIndexed(cmd, indexCount, 1, startIndexLocation, baseVertexLocation, 0);
        }
        auto GpuDeviceVulkan::draw_instanced(CommandBuffer* cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdDraw(cmd, vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
        }
        auto GpuDeviceVulkan::draw_indexed_instanced(CommandBuffer* cb, uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdDrawIndexed(cmd, indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
        }
        auto GpuDeviceVulkan::draw_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto internal_state = static_cast<const GpuBufferVulkan*>(args);
            vkCmdDrawIndirect(cmd, internal_state->handle, args_offset, 1, (uint32_t)sizeof(IndirectDrawArgsInstanced));
        }
        auto GpuDeviceVulkan::draw_indexed_instanced_indirect(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto internal_state = static_cast<const GpuBufferVulkan*>(args);
            vkCmdDrawIndexedIndirect(cmd, internal_state->handle, args_offset, 1, sizeof(IndirectDrawArgsIndexedInstanced));
        }
        auto GpuDeviceVulkan::draw_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto args_internal = static_cast<const GpuBufferVulkan*>(args);
            auto count_internal = static_cast<const GpuBufferVulkan*>(count);
            vkCmdDrawIndirectCount(cmd, args_internal->handle, args_offset, count_internal->handle, count_offset, max_count, sizeof(IndirectDrawArgsInstanced));
        }
        auto GpuDeviceVulkan::draw_indexed_instanced_indirect_count(CommandBuffer* cb, const GpuBuffer* args, uint64_t args_offset, const GpuBuffer* count, uint64_t count_offset, uint32_t max_count) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto args_internal = static_cast<const GpuBufferVulkan*>(args);
            auto count_internal = static_cast<const GpuBufferVulkan*>(count);
            vkCmdDrawIndexedIndirectCount(cmd, args_internal->handle, args_offset, count_internal->handle, count_offset, max_count, sizeof(IndirectDrawArgsIndexedInstanced));
        }


        auto GpuDeviceVulkan::trace_rays(CommandBuffer* cb, RayTracingPipeline* rtpipeline, const std::array<u32, 3>& threads) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto vk_rt = reinterpret_cast<RayTracingPipelineVulkan*>(rtpipeline);
            vkCmdTraceRaysKHR(cmd, 
                &vk_rt->shader_table.raygen_shader_binding_table,
                &vk_rt->shader_table.miss_shader_binding_table,
                &vk_rt->shader_table.hit_shader_binding_table,
                &vk_rt->shader_table.callable_shader_binding_table,
                threads[0],
                threads[1],
                threads[2]);
        }

        auto GpuDeviceVulkan::trace_rays_indirect(CommandBuffer* cb, RayTracingPipeline* rtpipeline, u64 args_buffer_address) -> void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            auto vk_rt = reinterpret_cast<RayTracingPipelineVulkan*>(rtpipeline);
            vkCmdTraceRaysIndirectKHR(cmd,
                &vk_rt->shader_table.raygen_shader_binding_table,
                &vk_rt->shader_table.miss_shader_binding_table,
                &vk_rt->shader_table.hit_shader_binding_table,
                &vk_rt->shader_table.callable_shader_binding_table,
                args_buffer_address);
        }

        auto GpuDeviceVulkan::event_begin(const char* name, CommandBuffer* cb)->void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
            label.pLabelName = name;
            label.color[0] = 0.0f;
            label.color[1] = 0.0f;
            label.color[2] = 0.0f;
            label.color[3] = 1.0f;
            vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
        }

        auto GpuDeviceVulkan::event_end(CommandBuffer* cb)->void
        {
            auto cmd = static_cast<GpuCommandBufferVulkan*>(cb)->handle;
            vkCmdEndDebugUtilsLabelEXT(cmd);
        }

        auto GpuDeviceVulkan::destroy_resource(GpuResource* pResource) -> void
        {
            if (pResource == nullptr)
                return;
            if(!pResource->is_signaled()) 
                return;
            if (pResource->is_texture())
            {
                auto vk_image = ((GpuTextureVulkan*)pResource);
                if(vk_image->image != VK_NULL_HANDLE)
                { 
                    auto vkimg = vk_image->image;
                    vkDestroyImage(device, vkimg, nullptr);
                    vkimg = VK_NULL_HANDLE;
                }
            }
            else if (pResource->is_buffer())
            {
                auto vk_buffer = ((GpuBufferVulkan*)pResource);
                if (vk_buffer != nullptr && vk_buffer->handle)
                {
                    vkDestroyBuffer(device, vk_buffer->handle, nullptr);
                    vk_buffer->handle = VK_NULL_HANDLE;
                    
                    cb_mutex.try_lock();
                    if (vk_buffer->allocation != nullptr)
                    {
                        vmaFreeMemory(global_allocator, vk_buffer->allocation);
                        vk_buffer->allocation = nullptr;
                    }
                    cb_mutex.unlock();
                }
            }
            else if (pResource->is_acceleration_structure())
            {
                auto& handle = ((GpuRayTracingAccelerationVulkan*)pResource)->acc;
                if( handle != VK_NULL_HANDLE)
                { 
                    vkDestroyAccelerationStructureKHR(device, handle, nullptr);
                    handle = VK_NULL_HANDLE;
                }
            }
        }

        auto GpuDeviceVulkan::defer_release(std::function<void()> f) -> void
        {
            cb_mutex.lock();
            destroy_queue.push_back(DeferedReleaseResource{f,0});
            cb_mutex.unlock();
        }

        auto GpuDeviceVulkan::export_image(rhi::GpuTexture* image) -> std::vector<u8> 
        {
            auto export_data = std::vector<u8>{};
            
            auto vk_img = static_cast<GpuTextureVulkan*>(image)->image;

            auto [width, height, depth] = image->desc.extent;

            uint32 per_pixel_bytes = 1;
            switch (image->desc.format)
            {
            case PixelFormat::R8G8B8A8_UNorm:
            case PixelFormat::R8G8B8A8_UNorm_sRGB:
                per_pixel_bytes = 4;
                break;
            case PixelFormat::R32G32B32A32_Float:
                per_pixel_bytes = 4 * 4;
                break;
            case PixelFormat::R16G16B16A16_Float:
                per_pixel_bytes = 8 * 4;
                break;
            }
            auto image_bytes = width * height * per_pixel_bytes;
            export_data.resize(image_bytes);

            GpuBufferDesc buffer_desc = { image_bytes, BufferUsageFlags::TRANSFER_DST, GPU_TO_CPU };
            auto img_buffer = std::static_pointer_cast<GpuBufferVulkan>(create_buffer(buffer_desc, "Image Data Buffer", nullptr));

            std::vector< VkBufferImageCopy> buffer_img_copies;
            u32 offset = 0;
            for (int level = 0; level < 1; level++)
            {
                VkImageSubresourceLayers subresourceRange = {};
                subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresourceRange.layerCount = 1;
                subresourceRange.mipLevel = level;
                subresourceRange.baseArrayLayer = 0;
                VkBufferImageCopy	region = {};
                region.bufferOffset = offset;
                region.imageSubresource = subresourceRange;
                region.imageExtent = VkExtent3D{ std::max(u32(1),image->desc.extent[0] >> level),  std::max(u32(1),image->desc.extent[1] >> level) , std::max(u32(1),image->desc.extent[2] >> level) };
                buffer_img_copies.push_back(region);
            }

            setup_graphics_queue_cmd_buffer([&](VkCommandBuffer cmd) {
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    image,
                    AccessType::AnyShaderWrite,
                    AccessType::TransferRead,
                    ImageAspectFlags::COLOR,
                    true
                    }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);

                vkCmdCopyImageToBuffer(cmd, vk_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img_buffer->handle, buffer_img_copies.size(), buffer_img_copies.data());

                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    image,
                    AccessType::TransferRead,
                    AccessType::AnyShaderWrite,
                    ImageAspectFlags::COLOR,
                    true
                    }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);
                });
            
            cb_mutex.lock();
            void* mapped_data;
            vmaMapMemory(global_allocator, img_buffer->allocation, &mapped_data);
            char* dstData = static_cast<char*>(mapped_data);
            memcpy(export_data.data(), dstData, image_bytes);
            vmaUnmapMemory(global_allocator, img_buffer->allocation);
            cb_mutex.unlock();

            immediate_destroy_buffer(*img_buffer);

            return export_data;
        }

        auto GpuDeviceVulkan::blit_image(rhi::GpuTexture* src, rhi::GpuTexture* dst,CommandBuffer* cmd_buf) -> void
        {
            auto src_image = dynamic_cast<GpuTextureVulkan*>(src)->image;
            auto dst_image = dynamic_cast<GpuTextureVulkan*>(dst)->image;
            VkImageBlit img_blit = {};
            img_blit.dstOffsets[0] = {0};
            img_blit.dstOffsets[1] = {(i32)dst->desc.extent[0], (i32)dst->desc.extent[1],(i32)dst->desc.extent[2] };
            img_blit.srcOffsets[0] = {0};
            img_blit.srcOffsets[1] = {(i32)src->desc.extent[0], (i32)src->desc.extent[1],(i32)src->desc.extent[2] };
            VkImageSubresourceLayers subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.layerCount = 1;
            subresourceRange.mipLevel = 0;
            subresourceRange.baseArrayLayer = 0;
            img_blit.srcSubresource = subresourceRange;
            img_blit.dstSubresource = subresourceRange;
            if (cmd_buf)
            {
                auto vkcmd_buf = static_cast<GpuCommandBufferVulkan*>(cmd_buf);
                auto cmd = vkcmd_buf->handle;
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                     dst,
                     AccessType::Nothing,
                     AccessType::TransferWrite,
                     ImageAspectFlags::COLOR,
                     true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::AnyShaderWrite,
                   AccessType::TransferRead,
                   ImageAspectFlags::COLOR,
                   true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);

                vkCmdBlitImage(cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_blit, VK_FILTER_LINEAR);


                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    dst,
                    AccessType::TransferWrite,
                    AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                    ImageAspectFlags::COLOR,
                    false
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::TransferRead,
                   AccessType::AnyShaderWrite,
                   ImageAspectFlags::COLOR,
                   true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
            }
            else
            {
                setup_graphics_queue_cmd_buffer([&](VkCommandBuffer cmd) {
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                  dst,
                  AccessType::Nothing,
                  AccessType::TransferWrite,
                  ImageAspectFlags::COLOR,
                  true
                    }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::AnyShaderWrite,
                   AccessType::TransferRead,
                   ImageAspectFlags::COLOR,
                   true
                    }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);

                vkCmdBlitImage(cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &img_blit, VK_FILTER_LINEAR);


                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    dst,
                    AccessType::TransferWrite,
                    AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                    ImageAspectFlags::COLOR,
                    false
                    }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::TransferRead,
                   AccessType::AnyShaderWrite,
                   ImageAspectFlags::COLOR,
                   true
                   }, graphics_queue_setup_cb->family_index, graphics_queue_setup_cb->family_index);
                });
            }
        }

        auto GpuDeviceVulkan::set_name(GpuResource* pResource, const char* name) const -> void
        {
            if (pResource == nullptr || name == nullptr)
                return;
            VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
            info.pObjectName = name;
            if (pResource->is_texture())
            {
                info.objectType = VK_OBJECT_TYPE_IMAGE;
                info.objectHandle = (uint64_t)((const GpuTextureVulkan*)pResource)->image;
            }
            else if (pResource->is_buffer())
            {
                info.objectType = VK_OBJECT_TYPE_BUFFER;
                info.objectHandle = (uint64_t)((const GpuBufferVulkan*)pResource)->handle;
            }
            else if (pResource->is_acceleration_structure())
            {
                info.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
                info.objectHandle = (uint64_t)((const GpuRayTracingAccelerationVulkan*)pResource)->acc;
            }

            if (info.objectHandle == (uint64_t)VK_NULL_HANDLE)
                return;
            VkResult res = vkSetDebugUtilsObjectNameEXT(device, &info);
            assert(res == VK_SUCCESS);
        }

        void GpuDeviceVulkan::set_label(u64 handle,VkObjectType objType, const char* name)
        {
            VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
            info.pObjectName = name; 
            info.objectType = objType;
            info.objectHandle = handle;
            if (info.objectHandle == (uint64_t)VK_NULL_HANDLE)
                return;
            VkResult res = vkSetDebugUtilsObjectNameEXT(device, &info);
            assert(res == VK_SUCCESS);
        }
        
        auto GpuDeviceVulkan::copy_image(GpuBuffer* src, GpuTexture* dst, CommandBuffer* cmd_buf)->void
        {
            auto srcbuffer = dynamic_cast<GpuBufferVulkan*>(src)->handle;
            auto dstimage = dynamic_cast<GpuTextureVulkan*>(dst)->image;
 
            u64 offset = 0;
            std::vector< VkBufferImageCopy> buffer_img_copies;
            for (int level = 0; level < 1; level++)
            {
                VkImageSubresourceLayers subresourceRange = {};
                subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresourceRange.layerCount = 1;
                subresourceRange.mipLevel = level;
                subresourceRange.baseArrayLayer = 0;
                VkBufferImageCopy	region = {};
                region.bufferOffset = offset;
                region.imageSubresource = subresourceRange;
                region.imageExtent = VkExtent3D{ std::max(u32(1),dst->desc.extent[0] >> level),  std::max(u32(1),dst->desc.extent[1] >> level) , std::max(u32(1),dst->desc.extent[2] >> level) };
                buffer_img_copies.push_back(region);
            }
            if(cmd_buf)
            { 
                auto vkcmd_buf = static_cast<GpuCommandBufferVulkan*>(cmd_buf);
                auto cmd = vkcmd_buf->handle;
                 {
                    rhi::record_image_barrier(*this, cmd, ImageBarrier{
                        dst,
                        AccessType::Nothing,
                        AccessType::TransferWrite,
                        ImageAspectFlags::COLOR,
                        true
                        }, vkcmd_buf->family_index, vkcmd_buf->family_index);

                    vkCmdCopyBufferToImage(cmd, srcbuffer, dstimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, buffer_img_copies.size(), buffer_img_copies.data());

                    rhi::record_image_barrier(*this, cmd, ImageBarrier{
                        dst,
                        AccessType::TransferWrite,
                        AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                        ImageAspectFlags::COLOR,
                        false
                        }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                 }
            }
            else
            {
                setup_cmd_buffer([&](VkCommandBuffer vkcmd) {
                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                        dst,
                        AccessType::Nothing,
                        AccessType::TransferWrite,
                        ImageAspectFlags::COLOR,
                        true
                        }, setup_cb->family_index, setup_cb->family_index);

                    vkCmdCopyBufferToImage(vkcmd, srcbuffer, dstimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, buffer_img_copies.size(), buffer_img_copies.data());

                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                        dst,
                        AccessType::TransferWrite,
                        AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                        ImageAspectFlags::COLOR,
                        false
                        }, setup_cb->family_index, setup_cb->family_index);
                    });
            }
        }

        auto GpuDeviceVulkan::copy_image(GpuTexture* src, GpuTexture* dst, CommandBuffer* cmd_buf)->void
        {
            auto srcimage = dynamic_cast<GpuTextureVulkan*>(src)->image;
            auto dstimage = dynamic_cast<GpuTextureVulkan*>(dst)->image;
//            assert(src->desc.format == dst->desc.format, "texture copy must have same format");
            //TODO:
            VkImageCopy image_copy = {};
            image_copy.dstOffset = {0};
            image_copy.srcOffset = {0};
            image_copy.extent = VkExtent3D{
                dst->desc.extent[0],
                dst->desc.extent[1],
                dst->desc.extent[2]
            };
            
            image_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_copy.srcSubresource.layerCount = 1;
            image_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_copy.dstSubresource.layerCount = 1;
            if(cmd_buf)
            {
                auto vkcmd_buf = static_cast<GpuCommandBufferVulkan*>(cmd_buf);
                auto cmd = vkcmd_buf->handle;
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    dst,
                    AccessType::Nothing,
                    AccessType::TransferWrite,
                    ImageAspectFlags::COLOR,
                    true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::AnyShaderWrite,
                   AccessType::TransferRead,
                   ImageAspectFlags::COLOR,
                   true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                vkCmdCopyImage(cmd, srcimage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstimage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    dst,
                    AccessType::TransferWrite,
                    AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                    ImageAspectFlags::COLOR,
                    false
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                   src,
                   AccessType::TransferRead,
                   AccessType::AnyShaderWrite,
                   ImageAspectFlags::COLOR,
                   true
                    }, vkcmd_buf->family_index, vkcmd_buf->family_index);
            }
            else
            {
                setup_cmd_buffer([&](VkCommandBuffer vkcmd) {
                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                         dst,
                         AccessType::Nothing,
                         AccessType::TransferWrite,
                         ImageAspectFlags::COLOR,
                         true
                        }, setup_cb->family_index, setup_cb->family_index);
                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                          src,
                          AccessType::AnyShaderWrite,
                          AccessType::TransferRead,
                          ImageAspectFlags::COLOR,
                          true
                        }, setup_cb->family_index, setup_cb->family_index);

                    vkCmdCopyImage(vkcmd, srcimage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstimage, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                        dst,
                        AccessType::TransferWrite,
                        AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                        ImageAspectFlags::COLOR,
                        false
                        }, setup_cb->family_index, setup_cb->family_index);
                    rhi::record_image_barrier(*this, vkcmd, ImageBarrier{
                      src,
                      AccessType::TransferRead,
                      AccessType::AnyShaderWrite,
                      ImageAspectFlags::COLOR,
                      true
                        }, setup_cb->family_index, setup_cb->family_index);

               });
            }
           
        }

        std::unordered_map<rhi::GpuTexture*,std::shared_ptr<rhi::GpuBufferVulkan>> updated_tex_buffers;

        auto GpuDeviceVulkan::update_texture(GpuTexture* image, const std::vector<ImageSubData>& initial_data, const TextureRegion& tex_region) -> void
        {
            const auto& desc = image->desc;
            std::shared_ptr<rhi::GpuBufferVulkan> staging_buffer;
            if(updated_tex_buffers.find(image) != updated_tex_buffers.end())
                staging_buffer = updated_tex_buffers[image];
            else
            {
                uint32 total_initial_data_bytes = 0;
                if (!initial_data.empty())
                {
                    for (const auto& d : initial_data)
                        total_initial_data_bytes += d.size;
                    uint32 block_bytes = 1;
                    switch (desc.format)
                    {
                    case PixelFormat::R8G8B8A8_UNorm:
                    case PixelFormat::R8G8B8A8_UNorm_sRGB:
                    case PixelFormat::R32G32B32A32_Float:
                    case PixelFormat::R16G16B16A16_Float:
                        block_bytes = 1;
                        break;
                    case    PixelFormat::BC1_UNorm_sRGB:
                    case    PixelFormat::BC1_UNorm:
                        block_bytes = 8;
                        break;
                    case PixelFormat::BC3_UNorm:
                    case PixelFormat::BC3_UNorm_sRGB:
                    case PixelFormat::BC5_UNorm:
                    case PixelFormat::BC5_SNorm:
                        block_bytes = 16;
                        break;
                    default:
                        block_bytes = 1;
                        break;
                    }
                }
                GpuBufferDesc buffer_desc = { total_initial_data_bytes, BufferUsageFlags::TRANSFER_SRC, CPU_TO_GPU };
                staging_buffer = std::static_pointer_cast<GpuBufferVulkan>(create_buffer(buffer_desc, "Staging Buffer", nullptr));     
                updated_tex_buffers[image] = staging_buffer;
            }
            auto vk_image = dynamic_cast<GpuTextureVulkan*>(image);
            uint32 offset = 0;
            void* mapped_data;
            cb_mutex.lock();
            vmaMapMemory(global_allocator, staging_buffer->allocation, &mapped_data);
            char* dstData = static_cast<char*>(mapped_data);
            std::vector< VkBufferImageCopy> buffer_img_copies;
            for (int level = 0; level < initial_data.size(); level++)
            {
                auto sub = initial_data[level];
                assert(offset % block_bytes == 0);
                memcpy(dstData + offset, sub.data, sub.size);

                VkImageSubresourceLayers subresourceRange = {};
                subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresourceRange.layerCount = 1;
                subresourceRange.mipLevel = level;
                subresourceRange.baseArrayLayer = 0;
                VkBufferImageCopy	region = {};
                region.bufferOffset = offset;
                region.imageSubresource = subresourceRange;
                region.imageExtent = VkExtent3D{ std::max(u32(1),desc.extent[0] >> level),  std::max(u32(1),desc.extent[1] >> level) , std::max(u32(1),desc.extent[2] >> level) };
                buffer_img_copies.push_back(region);
                offset += sub.size;
            };

            vmaUnmapMemory(global_allocator, staging_buffer->allocation);
            cb_mutex.unlock();

            setup_cmd_buffer([&](VkCommandBuffer cmd) {
                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    vk_image,
                    AccessType::Nothing,
                    AccessType::TransferWrite,
                    ImageAspectFlags::COLOR,
                    true
                    }, setup_cb->family_index, setup_cb->family_index);

                vkCmdCopyBufferToImage(cmd, staging_buffer->handle, vk_image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, buffer_img_copies.size(), buffer_img_copies.data());

                rhi::record_image_barrier(*this, cmd, ImageBarrier{
                    vk_image,
                    AccessType::TransferWrite,
                    AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer,
                    ImageAspectFlags::COLOR,
                    false
                    }, setup_cb->family_index, setup_cb->family_index);
            });
        }
    }
}
