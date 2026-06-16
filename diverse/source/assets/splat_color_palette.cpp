#include "splat_color_palette.h"
#include "backend/drs_rhi/gpu_device.h"

namespace diverse
{
    namespace
    {
        auto get_palette_device(const std::shared_ptr<rhi::GpuBuffer>& buffer)->rhi::GpuDevice*
        {
            return buffer ? buffer->get_owner_device() : nullptr;
        }
    }

     void SplatPaintColorPalette::allocate(u32 size, rhi::GpuDevice* device)
    {
        colors.reserve(size);
        if (colors.empty())
            add_paint_color(glm::vec4(1.0f), device);
        else if (device)
            upload(device);
    }

    void SplatPaintColorPalette::set_paint_color(u32 color_index, const glm::vec4& t, rhi::GpuDevice* device)
    {
        colors[color_index] = t;
        if (!device)
            device = get_palette_device(splat_color_buffer);
        if (device && splat_color_buffer)
            splat_color_buffer->copy_from(device, (u8*)&colors[color_index], sizeof(glm::vec4), color_index * sizeof(glm::vec4));
    }

    void SplatPaintColorPalette::get_paint_color(u32 color_index, glm::vec4& t)
    {
        t = colors[color_index];
    }

    glm::vec4 SplatPaintColorPalette::get_paint_color(u32 color_index)
    {
		return colors[color_index];
	}

    u32 SplatPaintColorPalette::add_paint_color(const glm::vec4& t, rhi::GpuDevice* device)
    {
        auto idx = colors.size();
        colors.push_back(t);
        auto bytes = colors.size() * sizeof(glm::vec4);
        if (!device)
            device = get_palette_device(splat_color_buffer);
        if (!device)
            return idx;
        if (!splat_color_buffer || bytes > splat_color_buffer->desc.size)
        {
            splat_color_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(bytes,
                rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_color_buffer", nullptr);
        }
        splat_color_buffer->copy_from(device, (u8*)glm::value_ptr(colors.back()), sizeof(glm::vec4), (colors.size() - 1) * sizeof(glm::vec4));
        return idx;
    }

    void SplatPaintColorPalette::upload(rhi::GpuDevice* device)
    {
        if (!device)
            device = get_palette_device(splat_color_buffer);
        if (!device || colors.empty())
            return;
        auto bytes = colors.size() * sizeof(glm::vec4);
        if (!splat_color_buffer || bytes > splat_color_buffer->desc.size)
        {
            splat_color_buffer = device->create_buffer(rhi::GpuBufferDesc::new_cpu_to_gpu(bytes,
                rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::TRANSFER_DST), "splat_color_buffer", nullptr);
        }
        if (splat_color_buffer)
            splat_color_buffer->copy_from(device, (u8*)colors.data(),colors.size() * sizeof(glm::vec4),0);
    }
}
