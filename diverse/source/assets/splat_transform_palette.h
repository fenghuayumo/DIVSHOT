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
    struct SplatTransformPalette
    {
        SplatTransformPalette(u32 alloc_size = 512)
        {
            allocate(alloc_size);
        }

        void allocate(u32 size);

        //update gpu buffer
        void upload();

        u32 add_transform(const glm::mat4& transform);

        void clear()
        {
            transforms.clear();
        }

        const std::vector<glm::mat3x4>& get_transforms() const
        {
            return transforms;
        }

        void set_transforms(const std::vector<glm::mat3x4>& transforms)
        {
            this->transforms = transforms;
        }

        void set_transform(u32 transform_index,const glm::mat4& t);
        void get_transform(u32 transform_index,glm::mat4& t);
        glm::mat4 get_transform(u32 transform_index);
        void set_front_transform(const glm::mat4& t);
        void set_back_transform(const glm::mat4& t);

        glm::mat3x4& operator[](size_t index)
        {
            return transforms[index];
        }

        const glm::mat3x4& operator[](size_t index) const
        {
            return transforms[index];
        }

        glm::mat3x4& back()
        {
            return transforms.back();
        }

        const glm::mat3x4& back() const
        {
            return transforms.back();
        }

        glm::mat3x4& front()
        {
            return transforms.front();
        }

        const glm::mat3x4& front() const
        {
            return transforms.front();
        }

        size_t size() const
        {
            return transforms.size();
        }

        std::shared_ptr<rhi::GpuBuffer> splat_transform_buffer;
    protected:
        std::vector<glm::mat3x4> transforms;
    };

}