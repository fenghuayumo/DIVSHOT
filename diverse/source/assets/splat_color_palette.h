#pragma once

#include <vector>
#include <memory>
#include "maths/transform.h"

namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
    }
    struct SplatPaintColorPalette
    {
        SplatPaintColorPalette(u32 alloc_size = 512)
        {
            allocate(alloc_size);
        }

        void allocate(u32 size);

        //update gpu buffer
        void upload();

        u32 add_paint_color(const glm::vec4& transform);

        void clear()
        {
        }

        const std::vector<glm::vec4>& get_colors() const
        {
            return colors;
        }

        void set_colors(const std::vector<glm::vec4>& colors)
        {
            this->colors = colors;
        }

        void set_paint_color(u32 color_index,const glm::vec4& t);
        void get_paint_color(u32 color_index,glm::vec4& t);
        glm::vec4 get_paint_color(u32 color_index);

        glm::vec4& operator[](size_t index)
        {
            return colors[index];
        }

        const glm::vec4& operator[](size_t index) const
        {
            return colors[index];
        }

        glm::vec4& back()
        {
            return colors.back();
        }

        const glm::vec4& back() const
        {
            return colors.back();
        }

        glm::vec4& front()
        {
            return colors.front();
        }

        const glm::vec4& front() const
        {
            return colors.front();
        }

        size_t size() const
        {
            return colors.size();
        }

        std::shared_ptr<rhi::GpuBuffer> splat_color_buffer;
    protected:
        std::vector<glm::vec4> colors;
    };

}