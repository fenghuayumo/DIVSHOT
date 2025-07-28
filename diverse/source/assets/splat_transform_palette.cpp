#include "splat_transform_palette.h"
#include "backend/drs_rhi/gpu_device.h"

namespace diverse
{
    void SplatTransformPalette::allocate(u32 size)
    {
        transforms.reserve(size);
        //create splat transform gpu buffer
        auto device = get_global_device();
        splat_transform_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(size * sizeof(glm::mat3x4),
            rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_transform_buffer", nullptr);

       add_transform(glm::mat4(1.0f));
    }

    void SplatTransformPalette::set_transform(u32 transform_index, const glm::mat4& t)
    {
        transforms[transform_index] = glm::transpose(t);
        splat_transform_buffer->copy_from(get_global_device(), (u8*)&transforms[transform_index], sizeof(glm::mat3x4), transform_index * sizeof(glm::mat3x4));
    }

    void SplatTransformPalette::get_transform(u32 transform_index, glm::mat4& t)
    {
        t = glm::transpose(transforms[transform_index]);
    }

    glm::mat4 SplatTransformPalette::get_transform(u32 transform_index)
    {
		return glm::transpose(transforms[transform_index]);
	}

    u32 SplatTransformPalette::add_transform(const glm::mat4& t)
    {
        auto idx = transforms.size();
        transforms.push_back(glm::transpose(t));
        auto bytes = transforms.size() * sizeof(glm::mat3x4);
        auto device = get_global_device();
        if (bytes > splat_transform_buffer->desc.size)
        {
            splat_transform_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(bytes,
                rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_transform_buffer", nullptr);
        }
        splat_transform_buffer->copy_from(device, (u8*)glm::value_ptr(transforms.back()), sizeof(glm::mat3x4), (transforms.size() - 1) * sizeof(glm::mat3x4));
        return idx;
    }

    void SplatTransformPalette::upload()
    {
        splat_transform_buffer->copy_from(get_global_device(), (u8*)transforms.data(),transforms.size() * sizeof(glm::mat3x4),0);
    }

    void SplatTransformPalette::set_front_transform(const glm::mat4& t)
    {
		transforms.front() = glm::transpose(t);
		splat_transform_buffer->copy_from(get_global_device(), (u8*)glm::value_ptr(transforms.front()), sizeof(glm::mat3x4), 0);
	}

    void SplatTransformPalette::set_back_transform(const glm::mat4& t)
    {
		transforms.back() = glm::transpose(t);
		splat_transform_buffer->copy_from(get_global_device(), (u8*)glm::value_ptr(transforms.back()), sizeof(glm::mat3x4), (transforms.size() - 1) * sizeof(glm::mat3x4));
	}
}