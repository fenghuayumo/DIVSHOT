#pragma once
#include <string>
#include <optional>
#include <vector>
#include <array>
#include <any>
#include <string_view>
#include <unordered_map>
#include "gpu_cmd_buffer.h"
#include "gpu_texture.h"

#define MAX_DESCRIPTOR_SETS 4

namespace diverse
{
    namespace rhi
    {
        struct GpuRayTracingAcceleration;
        struct GpuBuffer;
        enum class ShaderPipelineStage
        {
            Vertex,
            Geometry,
            Pixel,
            Compute,
            RayGen,
            RayMiss,
            RayClosestHit,
            RayAnyHit
        };

        enum DescriptorSetLayoutCreateFlags
        {
            UPDATE_AFTER_BIND_POOL = 0x00000002,
            PUSH_DESCRIPTOR_BIT_KHR = 0x00000001,
            HOST_ONLY_BIT_VALVE = 0x00000004,
        };
        ENUM_CLASS_FLAGS(DescriptorSetLayoutCreateFlags)

        enum class DescriptorType 
        {  
            SAMPLER = 0,
            COMBINED_IMAGE_SAMPLER = 1,
            SAMPLED_IMAGE = 2,
            STORAGE_IMAGE = 3,
            UNIFORM_TEXEL_BUFFER = 4,
            STORAGE_TEXEL_BUFFER = 5,
            UNIFORM_BUFFER = 6,
            STORAGE_BUFFER = 7,
            UNIFORM_BUFFER_DYNAMIC = 8,
            STORAGE_BUFFER_DYNAMIC = 9,
            INPUT_ATTACHMENT = 10,
            INLINE_UNIFORM_BLOCK_EXT = 1000138000,
            ACCELERATION_STRUCTURE_KHR = 1000150000,
            ACCELERATION_STRUCTURE_NV = 1000165000
        };

        enum class DescriptorBindflag
        {
            UPDATE_AFTER_BIND = 1,
            UPDATE_UNUSED_WHILE_PENDING = 1 << 1,
            PARTIALLY_BOUND = 1 << 2,
            VARIABLE_DESCRIPTOR_COUNT = 1 << 3
        };
        ENUM_CLASS_FLAGS(DescriptorBindflag)


        struct DescriptorDimensionality 
        {
            enum 
            {
                Single,
                RuntimeArray,
                Array
            }ty;
            u32 value;
        };

        struct DescriptorInfo
        {
            DescriptorType ty;
            DescriptorDimensionality dimensionality;
            std::string name;
        };

        struct DescriptorSet
        {
            // std::unordered_map<u32, bool> has_update_element;
        };

        struct PushConstantInfo 
        {
           uint32  offset = 0;
           uint32  size = 0;
        };

        using DescriptorSetLayout = std::unordered_map<uint32, DescriptorInfo>;
        using StageDescriptorSetLayouts = std::unordered_map<uint32, DescriptorSetLayout>;

        struct DescriptorSetLayoutOpts
        {
            std::optional<DescriptorSetLayoutCreateFlags> flags;
            std::optional<DescriptorSetLayout>  replace;
        };

        struct DescriptorImageInfo
        {
            ImageLayout image_layout;
            GpuTextureView* view;

        };

        struct DescriptorBufferInfo
        {
            struct GpuBuffer* buffer;
            u64 offset;
            u64 range;
        };

        struct DescriptorSetBinding
        {
            // std::any 
            enum class Type
            {
                Image,
                ImageArray,
                Buffer,
                RayTracingAcceleration,
                DynamicBuffer,
                DynamicStorageBuffer
            }ty;

       /*     struct Image{struct GpuTexture* image;};
            struct ImageArray{std::vector<struct GpuTexture*> images;};
            struct Buffer{struct GpuBuffer* buffer;};
            struct RayTracingAcceleration{};
            struct DynamicBuffer
            {
                struct GpuBuffer* buffer;
                uint32 offset;
            };
            struct DynamicStorageBuffer
            {
                struct GpuBuffer* buffer;
                uint32 offset;
            };*/
            std::any binding;

            auto Image() -> DescriptorImageInfo&
            {
                return std::any_cast<DescriptorImageInfo&>(binding);
            }

            auto ImageArray() -> std::vector<DescriptorImageInfo>&
            {
                return std::any_cast<std::vector<DescriptorImageInfo>&>(binding);
            }

            auto Buffer() -> DescriptorBufferInfo&
            {
                return std::any_cast<DescriptorBufferInfo&>(binding);
            }

            auto DynamicBuffer() -> std::pair<GpuBuffer* , u32>&
            {
                return std::any_cast<std::pair<GpuBuffer*, u32>&>(binding);
            }

            auto DynamicStorageBuffer() -> std::pair<GpuBuffer*, u32>&
            {
                return std::any_cast<std::pair<GpuBuffer*, u32>&>(binding);
            }

            auto RayTracingAcceleration() -> rhi::GpuRayTracingAcceleration*
            {
                return std::any_cast<rhi::GpuRayTracingAcceleration*>(binding);
            }

            static auto Image(const DescriptorImageInfo& imag_info) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding ::Type::Image, imag_info};
            }

            static auto ImageArray(const std::vector<DescriptorImageInfo>& imag_infos) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding::Type::Image, imag_infos };
            }

            static auto Buffer(const DescriptorBufferInfo& buffer_info) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding::Type::Buffer, buffer_info };
            }

            static auto DynamicBuffer(const std::pair<GpuBuffer*, u32>& buffer_info) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding::Type::DynamicBuffer, buffer_info };
            }

            static auto DynamicStorageBuffer(const std::pair<GpuBuffer*, u32>& buffer_info) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding::Type::DynamicStorageBuffer, buffer_info };
            }

            static auto RayTracingAcceleration(rhi::GpuRayTracingAcceleration* rt) -> DescriptorSetBinding
            {
                return { DescriptorSetBinding::Type::RayTracingAcceleration, rt };
            }
        };

        struct ShaderSource
        {
            std::string				path;
            std::string				entry = "main";
            std::vector<std::pair<std::string, std::string>> defines;
        };

        struct ComputePipelineDesc 
        {
            std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> descriptor_set_opts;
            uint32 push_constant_bytes = 0;
            ShaderSource source;
            std::string_view name;
            auto inline with_push_constants(u32 size) -> ComputePipelineDesc&
            {
                push_constant_bytes = size;
                return *this;
            }
            auto inline with_shader_source(const ShaderSource& s)->ComputePipelineDesc&
            {
                source = s;
                return *this;
            }

            auto inline with_name(const char* c_name)->ComputePipelineDesc&
            {
                name = c_name;
                return *this;
            }
        };

        struct GpuPipeline
        {
            //virtual void bind_pipeline(CommandBuffer* cb) = 0;
            //virtual void bind_descriptor_set(CommandBuffer* cb, uint32 set_idx, const DescriptorSet& set, u32 dynamic_offset_count, u32* dynamic_offset) = 0;
            //virtual void bind_descriptor_set(CommandBuffer* cb, const std::vector<DescriptorSetBinding>& bindings, uint32 set_index) = 0;
            enum class PieplineType
            {
                Raster,
                Compute,
                RayTracing
            }ty;
            std::array<uint32, 3>   group_size;
        };
        using ComputePipeline = GpuPipeline;
        //struct ComputePipeline : public GpuPipeline
        //{
        //    std::array<uint32, 3>   group_size;
        //    // virtual void bind_pipeline() = 0;
        //};

        struct PipelineShaderDesc
        {
            ShaderPipelineStage		stage;
            std::optional<std::vector<std::pair<uint32, DescriptorSetLayoutCreateFlags>>>	descriptor_set_layout_flags;
            uint32				    push_constants_bytes = 0;
            ShaderSource source;

            auto inline with_stage(ShaderPipelineStage s) -> PipelineShaderDesc&
            {
                stage = s;
                return *this;
            }

            auto inline with_shader_source(ShaderSource s) -> PipelineShaderDesc&
            {
                source = s;
                return *this;
            }
        };
        enum class PrimitiveTopType
        {
            TriangleList,
            LineList,
            PointList,
            TriangleStrip,
        };

        enum class BlendMode : uint8_t
        {
            None = 0,
            OneZero,
            ZeroSrcColor,
            SrcAlphaOneMinusSrcAlpha,
            SrcAlphaOne,
            OneOneMinusSrcAlpha,
            OneOne
        };
        enum class CullMode : uint8_t
        {
            FRONT = 0,
            BACK,
            FRONTANDBACK,
            NONE
        };

        enum class FrontFaceOrder
        {
            CCW = 0,
            CW
        };
        enum class CompareFunc
        {
            Never = 0,
            Less = 1,
            Equal = 2,
            LessEqual = 3,
            Greater = 4,
            NotEqual = 5,
            GreaterEqaul = 6,
            Always = 7
        };
        enum class PolygonMode
        {
            Fill = 0,
            WireFrame = 1,
            Point = 2,
        };
        struct RasterPipelineDesc
        {
            std::array<std::optional<std::pair<uint32, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS> descriptor_set_opts;
            std::shared_ptr<struct RenderPass>  render_pass;
            //bool face_cull = true;
            CullMode cull_mode = CullMode::BACK;
            bool depth_write = true;
            bool depth_test = true;
            bool blend_enabled = false;
            f32  line_width = 1.0f;
            bool depth_bias_enabled = false;
            u32 push_constants_bytes = 0;
            PrimitiveTopType   primitive_type = PrimitiveTopType::TriangleList;
            BlendMode blend_mode = BlendMode::None;
            bool vextex_attribute = true;
            FrontFaceOrder  face_order = FrontFaceOrder::CCW;
            CompareFunc    depth_compare_op = CompareFunc::GreaterEqaul;
            PolygonMode    polygon_mode = PolygonMode::Fill;
            bool discard_enable = false;
            std::string_view name;
            auto inline with_cull_mode(CullMode cull) -> RasterPipelineDesc&
            {
                cull_mode = cull;
                return *this;
            }
            auto inline with_line_width(f32 line_w) -> RasterPipelineDesc&
            {
                line_width = line_w;
                return *this;
            }
            auto inline with_depth_write(bool write) -> RasterPipelineDesc&
            {
                depth_write = write;
                return *this;
            }

            auto inline with_discard_enable(bool val)->RasterPipelineDesc&
            {
                discard_enable = val;
                return *this;
            }

            auto with_face_order(FrontFaceOrder order) -> RasterPipelineDesc&
            {
                face_order = order;
                return *this;
            }
            auto inline with_depth_test(bool depthtest) -> RasterPipelineDesc&
            {
                depth_test = depthtest;
                return *this;
            }

            auto inline with_vetex_attribute(bool vertex_attr)->RasterPipelineDesc&
            {
                vextex_attribute = vertex_attr;
                return *this;
            }

            auto inline with_push_constants(u32 size) -> RasterPipelineDesc&
            {
                push_constants_bytes = size;
                return *this;
            }
            auto inline with_primitive_type(PrimitiveTopType type) -> RasterPipelineDesc&
            {
                primitive_type = type;
                return *this;
            }
            auto inline with_render_pass(const std::shared_ptr<struct RenderPass>& render_p) -> RasterPipelineDesc&
            {
                render_pass = render_p;
                return *this;
            }

            auto inline with_blend_mode(BlendMode bm) -> RasterPipelineDesc&
            {
                if( bm != BlendMode::None)
                    blend_enabled = true;
                blend_mode = bm;
                return *this;
            }

            auto inline with_polygon_mode(PolygonMode mode)->RasterPipelineDesc&
            {
                this->polygon_mode = mode;
                return *this;
            }

            auto inline with_depth_compare_op(CompareFunc depth_compare_op) -> RasterPipelineDesc&
            {
                this->depth_compare_op = depth_compare_op;
                return *this;
            }
            auto inline with_name(const char* name)->RasterPipelineDesc&
            {
                this->name = name;
                return *this;
            }
        };
        using RasterPipeline = GpuPipeline;
        //struct RasterPipeline : public GpuPipeline
        //{
        //    // virtual void bind_pipeline() = 0;
        //};

        struct CompiledShaderCode
        {
     /*       uint8*			shader_code;
            uint32			shader_byte_len;*/
            std::vector<u8>     codes;
        };

         //template<typename ShaderCode>
        struct PipelineShader
        {
            CompiledShaderCode code;
            // uint8*			shader_code;
            // uint32			shader_byte_len;
            PipelineShaderDesc desc;
            //virtual  auto get_descriptor_sets()->std::unordered_map<uint32, std::unordered_map<uint32, DescriptorInfo>> = 0;
        };

        enum class pipelineType
        {
            Compute,
            Raster,
            RayTracing
        };
    }
}

template<>
struct std::hash<diverse::rhi::ShaderSource>
{
    std::size_t operator()(diverse::rhi::ShaderSource const& s) const noexcept
    {
        u64 hash_code = std::hash<std::string>{}(s.entry);
        diverse::hash_combine(hash_code, (u64)std::hash<std::string>{}(s.path));
        for(const auto& def : s.defines)
        {
            diverse::hash_combine(hash_code, (u64)std::hash<std::string>{}(def.first));
            diverse::hash_combine(hash_code, (u64)std::hash<std::string>{}(def.second));
        }
        return hash_code;
    }
};

template<>
struct std::equal_to<diverse::rhi::ShaderSource>
{
    constexpr bool operator()(const diverse::rhi::ShaderSource& lhs, const diverse::rhi::ShaderSource& rhs) const
    {
        bool def_is_equal = lhs.defines.size() == rhs.defines.size();
        auto size = std::min(lhs.defines.size(), rhs.defines.size());
        for (auto i = 0; i < size; i++) {
            def_is_equal &= lhs.defines[i] == rhs.defines[i];
        }
        return lhs.entry == rhs.entry &&
               lhs.path == rhs.path;
    }
};


//template<>
//struct std::hash< std::vector<std::pair<uint32, diverse::rhi::DescriptorSetLayoutCreateFlags>> >
//{
//    std::size_t operator()(std::vector<std::pair<uint32, diverse::rhi::DescriptorSetLayoutCreateFlags>> const& s) const noexcept
//    {
//
//    }
//};

template<>
struct std::hash<diverse::rhi::PipelineShaderDesc>
{
    std::size_t operator()(diverse::rhi::PipelineShaderDesc const& s) const noexcept
    {
        u64 hash_code = std::hash<diverse::rhi::ShaderSource>{}(s.source);
        diverse::hash_combine(hash_code, uint32(s.stage));
        diverse::hash_combine(hash_code, uint32(s.push_constants_bytes));
        //hash_combine(hash_code, std::hash< std::vector<std::pair<uint32, diverse::rhi::DescriptorSetLayoutCreateFlags>> >{}(s.descriptor_set_layout_flags) );
        if (s.descriptor_set_layout_flags)
        {
            for (auto set_flag : s.descriptor_set_layout_flags.value())
            {
                diverse::hash_combine(hash_code, uint32(set_flag.first));
                diverse::hash_combine(hash_code, uint32(set_flag.second));
            }
        }

        return hash_code;
    }
};

template<>
struct std::hash<std::vector<diverse::rhi::PipelineShaderDesc>>
{
    std::size_t operator()(std::vector<diverse::rhi::PipelineShaderDesc> const& s) const noexcept
    {
        u64 hash_code = 0;
        for (auto ss : s)
        {
            diverse::hash_combine(hash_code, (u64)std::hash<diverse::rhi::PipelineShaderDesc>{}(ss));
        }
        return hash_code;
    }
};


template<>
struct std::equal_to<diverse::rhi::PipelineShaderDesc>
{
    constexpr bool operator()(const diverse::rhi::PipelineShaderDesc& lhs, const diverse::rhi::PipelineShaderDesc& rhs) const
    {
        return std::equal_to<diverse::rhi::ShaderSource>{}(lhs.source, rhs.source) &&
            lhs.stage == rhs.stage &&
            lhs.descriptor_set_layout_flags == rhs.descriptor_set_layout_flags &&
            lhs.push_constants_bytes == rhs.push_constants_bytes;
    }
};
template<>
struct std::equal_to<std::vector<diverse::rhi::PipelineShaderDesc>>
{
    constexpr bool operator()(const std::vector<diverse::rhi::PipelineShaderDesc>& lhs, const std::vector<diverse::rhi::PipelineShaderDesc>& rhs) const
    {
       bool ret = lhs.size() == rhs.size();
       if( !ret ) return false;
       for (auto i = 0; i < lhs.size(); i++)
       {
           ret &= std::equal_to<diverse::rhi::PipelineShaderDesc>{}(lhs[i], rhs[i]);
       }
       return ret;
    }
};

namespace diverse
{
    namespace rhi
    {
        auto merge_shader_stage_layout_pair(const StageDescriptorSetLayouts& src, StageDescriptorSetLayouts& dst) -> void;
        auto merge_shader_stage_layouts(const std::vector<StageDescriptorSetLayouts>& stages) -> StageDescriptorSetLayouts;
    }
}