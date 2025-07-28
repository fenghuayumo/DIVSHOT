#include "splat_color_palette.h"
#include "backend/drs_rhi/gpu_device.h"

namespace diverse
{
     void SplatPaintColorPalette::allocate(u32 size)
    {
        colors.reserve(size);
        //create splat transform gpu buffer
        auto device = get_global_device();
        splat_color_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(size * sizeof(glm::vec4),
            rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_color_buffer", nullptr);

       add_paint_color(glm::vec4(1.0f));
    }

    void SplatPaintColorPalette::set_paint_color(u32 color_index, const glm::vec4& t)
    {
        colors[color_index] = t;
        splat_color_buffer->copy_from(get_global_device(), (u8*)&colors[color_index], sizeof(glm::vec4), color_index * sizeof(glm::vec4));
    }

    void SplatPaintColorPalette::get_paint_color(u32 color_index, glm::vec4& t)
    {
        t = colors[color_index];
    }

    glm::vec4 SplatPaintColorPalette::get_paint_color(u32 color_index)
    {
		return colors[color_index];
	}

    u32 SplatPaintColorPalette::add_paint_color(const glm::vec4& t)
    {
        auto idx = colors.size();
        colors.push_back(t);
        auto bytes = colors.size() * sizeof(glm::vec4);
        auto device = get_global_device();
        if (bytes > splat_color_buffer->desc.size)
        {
            splat_color_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(bytes,
                rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_color_buffer", nullptr);
        }
        splat_color_buffer->copy_from(device, (u8*)glm::value_ptr(colors.back()), sizeof(glm::vec4), (colors.size() - 1) * sizeof(glm::vec4));
        return idx;
    }

    void SplatPaintColorPalette::upload()
    {
        splat_color_buffer->copy_from(get_global_device(), (u8*)colors.data(),colors.size() * sizeof(glm::vec4),0);
    }
}