#pragma once

#ifdef DS_VOLK
#define VK_NO_PROTOTYPES
#include <vulkan/volk/volk.h>
#else
#include <vulkan/vulkan.h>
#endif
#include <vulkan/vk_mem_alloc.h>
//
//inline PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
//inline PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
//inline PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
//inline PFN_vkBindBufferMemory vkBindBufferMemory;

std::string vk_error_string(VkResult errorCode);

#define VK_CHECK_RESULT(f)                                                                                                                        \
    {                                                                                                                                             \
        VkResult res = (f);                                                                                                                       \
        if(res != VK_SUCCESS)                                                                                                                     \
        {                                                                                                                                         \
            DS_LOG_ERROR("[VULKAN] : VkResult is {0} in {1} at line {2}", vk_error_string(res), __FILE__, __LINE__); \
        }                                                                                                                                         \
    }