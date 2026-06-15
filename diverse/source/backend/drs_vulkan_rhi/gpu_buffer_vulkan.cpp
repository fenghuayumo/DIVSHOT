#include "gpu_buffer_vulkan.h"
#include "gpu_device_vulkan.h"

namespace diverse
{
    namespace rhi
    {
        GpuBufferVulkan::GpuBufferVulkan(VkBuffer h,const GpuBufferDesc& desc,VmaAllocation  alloc, VmaAllocationInfo info, GpuDeviceVulkan* owner)
            : GpuBuffer(desc)
            ,owner_device(owner)
            ,handle(h),
            allocation(alloc),
            allocate_info(info)
        {
            data_buf = (u8*)allocate_info.pMappedData;
        }
        GpuBufferVulkan::~GpuBufferVulkan()
        {
            if(owner_device && handle != VK_NULL_HANDLE)
            {
                auto vk_device = owner_device->device;
                auto allocator = owner_device->global_allocator;
                owner_device->defer_release([handle = this->handle, allocation = this->allocation, vk_device, allocator]() mutable {
                    if( handle != VK_NULL_HANDLE){
                        vkDestroyBuffer(vk_device, handle, nullptr);
                        handle = VK_NULL_HANDLE;
                        if( allocation != nullptr)
                        {
                            vmaFreeMemory(allocator, allocation);
                            allocation = nullptr;
                        }
                    }
                });
            }
        }

        uint64	GpuBufferVulkan::device_address(const struct GpuDevice* device)
        {
            auto vk_device = dynamic_cast<const GpuDeviceVulkan*>(device);
            VkBufferDeviceAddressInfo bufferDeviceAI{};
            bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAI.buffer = handle;
            return vkGetBufferDeviceAddress(vk_device->device, &bufferDeviceAI);
        }

        auto GpuBufferVulkan::map(const GpuDevice* device) -> u8*
        {
            auto device_vk = static_cast<const GpuDeviceVulkan*>(device);
            VkResult res = static_cast<VkResult>(vmaMapMemory(device_vk->global_allocator, allocation, (void**)&data_buf));
            return data_buf;
        }

        auto GpuBufferVulkan::unmap(const GpuDevice* device) -> void
        {
            auto device_vk = static_cast<const GpuDeviceVulkan*>(device);
            vmaUnmapMemory(device_vk->global_allocator, allocation);
        }

        auto GpuBufferVulkan::view(const struct GpuDevice* device, const GpuBufferViewDesc& view_desc)->std::shared_ptr<GpuBufferView>
        {
            if (desc.usage & (BufferUsageFlags::STORAGE_TEXEL_BUFFER | BufferUsageFlags::UNIFORM_TEXEL_BUFFER) )
            {
                auto vk_device = dynamic_cast<const GpuDeviceVulkan*>(device);
                auto it = views.find(view_desc);
                if (it == views.end())
                {
                    auto buffer_view = vk_device->create_buffer_view(view_desc, desc, handle);
                    views[view_desc] = std::move(buffer_view);
                }
                return views[view_desc];
            }
            return nullptr;
        }

        GpuBufferViewVulkan::~GpuBufferViewVulkan()
        {
            if (buf_view != VK_NULL_HANDLE && owner_device)
            {
                auto vk_device = owner_device->device;
                owner_device->defer_release([buf_view = this->buf_view, vk_device]() mutable {
                    if (buf_view != VK_NULL_HANDLE)
                    {
                        vkDestroyBufferView(vk_device, buf_view, nullptr);
                        buf_view = VK_NULL_HANDLE;
                    }
                });
            }
        }
    }
}
