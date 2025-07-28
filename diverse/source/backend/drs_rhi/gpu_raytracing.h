#pragma once
#include "gpu_resource.h"
#include "gpu_pipeline.h"
#include <string_view>
#include <glm/mat3x4.hpp>
namespace diverse
{
    namespace rhi
    {
        struct GpuDevice;
        struct RayTracingAccelerationDesc
        {

        };

        struct RayTracingAccelerationScratchBuffer
        {
            std::shared_ptr<GpuBuffer>  buffer;
        };
        struct GpuRayTracingAcceleration : public GpuResource
        {
            using Desc = RayTracingAccelerationDesc;
            
            Desc desc;

            std::shared_ptr<GpuBuffer> backing_buffer;

            GpuRayTracingAcceleration(const  std::shared_ptr<GpuBuffer>& buffer)
            : backing_buffer(buffer)
            {
                ty = GpuResource::Type::RayTracingAcceleration;
            }

            virtual auto as_device_address(const GpuDevice* device)->u64 = 0;
        };


        struct RayTracingPipelineDesc
        {
            std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> descriptor_set_opts;
            uint32 max_pipeline_ray_recursion_depth = 1;
            std::string_view    name;
        };

        struct RayTracingShaderTableDesc
        {
            uint32 raygen_entry_count;
            uint32 hit_entry_count;
            uint32 miss_entry_count;
        };
        
        struct RayTracingGeometryPart
        {
            u64 vertex_count;
            u64 vertex_buffer_address;
            u64 index_count;
            u64 index_buffer_address;
            u32 max_vertex;
        };

        enum class RayTracingGeometryType
        {
            Triangle = 0,
            BoundingBox = 1
        };

        enum GeometryInstanceFlag
        {
            TRIANGLE_FACING_CULL_DISABLE = 1,
            TRIANGLE_FLIP_FACING = 1 << 1,
            FORCE_OPAQUE = 1 << 2,
            FORCE_NO_OPAQUE = 1 << 3,
            TRIANGLE_FRONT_COUNTERCLOCKWISE = TRIANGLE_FLIP_FACING,
        };
        struct GeometryInstance
        {
            glm::mat3x4 transform;
            u32 instance_id_and_mask;
            u32 instance_sbt_offset_and_flags;
            u64 blas_address;

            GeometryInstance(const glm::mat3x4& t,
                u32 id,
                u8 mask,
                u32 sbt_offset,
                GeometryInstanceFlag flag,
                u64 address)
                : transform(t),
                instance_id_and_mask(0),
                instance_sbt_offset_and_flags(0),
                blas_address(address)
            {
                set_id(id);
                set_mask(mask);
                set_sbt_offset(sbt_offset);
                set_flags(flag);
            }

            void set_id(u32 id)
            {
                id = id & 0x00ffffff;
                instance_id_and_mask |= id;
            }

            void set_mask(u8 mask)
            {
                mask = (u32)mask;
                instance_id_and_mask |= mask << 24;
            }

            auto set_sbt_offset(u32 offset)->void
            {
                offset = offset & 0x00ffffff;
                instance_sbt_offset_and_flags |= offset;
            }

            auto set_flags(GeometryInstanceFlag flag)->void
            {
                auto flags = (u32)flag;
                instance_sbt_offset_and_flags |= flags << 24;
            }
        };

        struct RayTracingGeometryDesc
        {
            RayTracingGeometryType geometry_type;
            PixelFormat vertex_format;
            u64 vertex_stride;
            std::vector<RayTracingGeometryPart> parts;
        };

        struct RayTracingInstanceDesc
        {
            std::shared_ptr<GpuRayTracingAcceleration>  blas;
            glm::mat3x4 transformation;
            u32 mesh_index;
            u8  mask = 0xff;
        };

        struct RayTracingTopAccelerationDesc
        {
            std::vector< RayTracingInstanceDesc>    instances;
            u64 preallocate_bytes;
        };

        struct RayTracingBottomAccelerationDesc
        {
            std::vector< RayTracingGeometryDesc>  geometries;
        };

        using RayTracingPipeline = GpuPipeline;

    }
}


template<>
struct fmt::formatter<diverse::rhi::RayTracingShaderTableDesc> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::RayTracingShaderTableDesc v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "RayTracingShaderTableDesc raygen_entry_count: {}, hit_entry_count: {}, miss_entry_count: {}", 
            v.raygen_entry_count, v.hit_entry_count, v.miss_entry_count);
    }
};