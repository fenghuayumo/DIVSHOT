#pragma once
#include "gpu_resource.h"

namespace diverse
{
    namespace rhi
    {
        enum class BufferUsageFlags
        {
            TRANSFER_SRC = 1,
            TRANSFER_DST = 1 << 1,
            UNIFORM_TEXEL_BUFFER = 1 << 2,
            STORAGE_TEXEL_BUFFER = 1 << 3,
            UNIFORM_BUFFER = 1 << 4,
            STORAGE_BUFFER = 1 << 5,
            INDEX_BUFFER = 1 << 6,
            VERTEX_BUFFER = 1 << 7,
            INDIRECT_BUFFER = 1 << 8,
            SHADER_DEVICE_ADDRESS = 1 << 17,
            ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR =  1 << 29,
            ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY = 1 << 19,
            ACCELERATION_STRUCTURE_STORAGE_KHR = 1 << 20,
            SHADER_BINDING_TABLE = 1 << 10
        };
        ENUM_CLASS_FLAGS(BufferUsageFlags)

        enum MemoryUsage
        {
            Unknown = 0,
            GPU_ONLY = 1,
            CPU_ONLY = 2,
            CPU_TO_GPU = 3,
            GPU_TO_CPU = 4,
            CPU_COPY = 5,
        };

        struct GpuBufferDesc 
        {
            uint64		size;
            BufferUsageFlags	usage;
            MemoryUsage		memory_usage;
            uint64 align;
            PixelFormat		format = PixelFormat::Unknown;
            inline static GpuBufferDesc	new_gpu_only(uint64 size, BufferUsageFlags	usage) {
                return {size, usage,GPU_ONLY, 0 };
            }

            inline static GpuBufferDesc	new_cpu_to_gpu(uint64 size, BufferUsageFlags	usage) {
                return { size, usage,CPU_TO_GPU, 0 };
            }

            inline static GpuBufferDesc	new_gpu_to_cpu(uint64 size, BufferUsageFlags	usage) {
                return { size, usage,GPU_TO_CPU, 0 };
            }

            inline GpuBufferDesc&	alignment(uint64 align) {
                this->align = align;
                return *this;
            }

            inline auto with_size(u64 si) -> GpuBufferDesc&
            {
                this->size = si;
                return *this;
            }

            inline auto with_format(PixelFormat f) -> GpuBufferDesc&
            {
                this->format = f;
                return *this;
            }
        };

        struct GpuBufferViewDesc
        {
            PixelFormat		format = PixelFormat::R8G8B8A8_UNorm;
            u64             offset = 0;
            u64             range = (~0ULL);
        };
        struct GpuBufferView
        {
            virtual ~GpuBufferView();
        };
        struct GpuBuffer : public GpuResource
        {
            GpuBuffer(const GpuBufferDesc& des) : desc(des){ ty = GpuResource::Type::Buffer; }
            GpuBuffer(){ ty = GpuResource::Type::Buffer;}
            virtual ~GpuBuffer();
            using Desc = GpuBufferDesc;
            // VkBuffer	handle;
            Desc	desc;
            u8*     data_buf;

            virtual uint64 device_address(const struct GpuDevice* device) = 0;
            virtual auto view(const struct GpuDevice* device,const GpuBufferViewDesc& view_desc) -> std::shared_ptr<GpuBufferView> = 0;
            u8* data(u32 offset) { return data_buf + offset; }
            virtual void copy_from(const struct GpuDevice* device,const u8* src_data, u32 src_bytes, u32 dst_offset);
            virtual void copy_to(const struct GpuDevice* device,u8* dst_data, u32 dst_bytes, u32 src_offset);
            virtual auto map(const struct GpuDevice* device)->u8* = 0;
            virtual auto unmap(const struct GpuDevice* device)->void = 0;
            auto export_data()->std::vector<u8>;
        };

        inline auto operator==(const GpuBufferDesc& s1, const GpuBufferDesc& desc)->bool
        {
            return s1.size == desc.size
                && s1.usage == desc.usage
                && s1.memory_usage == desc.memory_usage
                && s1.align == desc.align
                && s1.format == desc.format;
        }
        inline auto operator==(const GpuBufferViewDesc& s1, const GpuBufferViewDesc& s2)-> bool
        {
            return s1.format == s2.format
                && s1.offset == s2.offset
                && s1.range == s2.range;
        }
        auto buffer_access_type_to_usage_flags(AccessType access_type)-> BufferUsageFlags;
    }
}
template<>
struct std::hash<diverse::rhi::GpuBufferViewDesc>
{
    std::size_t operator()(diverse::rhi::GpuBufferViewDesc const& s) const noexcept
    {
        auto hash_code = diverse::hash<u32>()(u32(s.format));
        diverse::hash_combine(hash_code, s.offset);
        diverse::hash_combine(hash_code, s.range);
        return hash_code;
    }
};
template<>
struct std::hash<diverse::rhi::GpuBufferDesc>
{
	std::size_t operator()(diverse::rhi::GpuBufferDesc const& s) const noexcept
	{
		auto hash_code = diverse::hash<u32>()(u32(s.size));
        diverse::hash_combine(hash_code, u32(s.usage));
        diverse::hash_combine(hash_code, u32(s.memory_usage));
        diverse::hash_combine(hash_code, u32(s.align));
        diverse::hash_combine(hash_code, u32(s.format));
		return hash_code;
	}
};

template<>
struct fmt::formatter<diverse::rhi::BufferUsageFlags> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::BufferUsageFlags value, format_context& ctx) const -> decltype(ctx.out())
    {
        std::string str;
        switch (value)
        {
        case diverse::rhi::BufferUsageFlags::TRANSFER_SRC:
            str = "BufferUsageFlags::TRANSFER_SRC";
            break;
        case diverse::rhi::BufferUsageFlags::TRANSFER_DST:
            str = "BufferUsageFlags::TRANSFER_DST";
            break;
        case diverse::rhi::BufferUsageFlags::UNIFORM_TEXEL_BUFFER:
            str = "BufferUsageFlags::UNIFORM_TEXEL_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::STORAGE_TEXEL_BUFFER:
            str = "BufferUsageFlags::STORAGE_TEXEL_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::UNIFORM_BUFFER:
            str = "BufferUsageFlags::UNIFORM_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::STORAGE_BUFFER:
            str = "BufferUsageFlags::STORAGE_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::INDEX_BUFFER:
            str = "BufferUsageFlags::INDEX_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::VERTEX_BUFFER:
            str = "BufferUsageFlags::VERTEX_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::INDIRECT_BUFFER:
            str = "BufferUsageFlags::INDIRECT_BUFFER";
            break;
        case diverse::rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS:
            str = "BufferUsageFlags::SHADER_DEVICE_ADDRESS";
            break;
        case diverse::rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR:
            str = "BufferUsageFlags::ACCELERATION_STRUCTURE_VERTEX_BUFFER_KHR";
            break;
        case diverse::rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY:
            str = "BufferUsageFlags::ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY";
            break;
        case diverse::rhi::BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR:
            str = "BufferUsageFlags::ACCELERATION_STRUCTURE_STORAGE_KHR";
            break;
        case diverse::rhi::BufferUsageFlags::SHADER_BINDING_TABLE:
            str = "BufferUsageFlags::SHADER_BINDING_TABLE";
            break;
        default:
            break;
        }

        return fmt::format_to(ctx.out(), "{}", str);
    }
};

template<>
struct fmt::formatter<diverse::rhi::MemoryUsage> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::MemoryUsage v, format_context& ctx) const -> decltype(ctx.out())
    {
        std::string str;
        switch (v)
        {
        case diverse::rhi::Unknown:
            str = "MemoryUsage::Unkown";
            break;
        case diverse::rhi::GPU_ONLY:
            str = "MemoryUsage::GPU_ONLY";
            break;
        case diverse::rhi::CPU_ONLY:
            str = "MemoryUsage::CPU_ONLY";
            break;
        case diverse::rhi::CPU_TO_GPU:
            str = "MemoryUsage::CPU_TO_GPU";
            break;
        case diverse::rhi::GPU_TO_CPU:
            str = "MemoryUsage::GPU_TO_CPU";
            break;
        case diverse::rhi::CPU_COPY:
            str = "MemoryUsage::CPU_COPY";
            break;
        default:
            break;
        }
        return fmt::format_to(ctx.out(), "{}", str);
    }
};

template<>
struct fmt::formatter<diverse::rhi::GpuBufferDesc> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::GpuBufferDesc v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "GpuBufferDesc size: {}, usage: {}, memory_usage :{}, align: {}",
            v.size, v.usage, v.memory_usage, v.align);
    }
};
