#pragma once
#include "gpu_resource.h"
#include <array>
namespace diverse
{
    namespace rhi
    {
        inline u32 mip_count_1d(u32 extent)
        {
            return static_cast<u32>(std::floorf(std::log2f(extent))) + 1;
        }
        enum TextureType 
        {
            Tex1d = 0,
            Tex1dArray = 1,
            Tex2d = 2,
            Tex2dArray = 3,
            Tex3d = 4,
            Cube = 5,
            CubeArray = 6,
        };

        enum class ImageAspectFlags
        {
            COLOR = 0x00000001,
            DEPTH = 0x00000002,
            STENCIL = 0x00000004,
            METADATA = 0x00000008,
        };
        ENUM_CLASS_FLAGS(ImageAspectFlags)

    	struct GpuTextureViewDesc
        {
            std::optional<TextureType>	view_type;
            PixelFormat		format = PixelFormat::R8G8B8A8_UNorm;
            ImageAspectFlags	aspect_mask = ImageAspectFlags::COLOR;
            u32		base_mip_level = 0;
            u32		level_count = 0;

            auto with_aspect_mask(ImageAspectFlags aspect) -> GpuTextureViewDesc&
            {
                aspect_mask = aspect;
                return *this;
            }
            auto with_view_type(const TextureType& ty)-> GpuTextureViewDesc&
            {
                view_type = ty;
                return *this;
            }
            auto with_base_mip_level(u32 mip_level) -> GpuTextureViewDesc&
            {
                base_mip_level = mip_level;
                return *this;
            }
            auto with_level_count(u32 level_cnt) -> GpuTextureViewDesc&
            {
                level_count = level_cnt;
                return *this;
            }
        };

        enum class TextureUsageFlags
        {
            TRANSFER_SRC = 1,
            TRANSFER_DST = 1 << 1,
            SAMPLED =  1 << 2,
            STORAGE =  1 << 3,
            COLOR_ATTACHMENT = 1 << 4,
            DEPTH_STENCIL_ATTACHMENT = 1 << 5,
            TRANSIENT_ATTACHMENT = 1 << 6,
            INPUT_ATTACHMENT = 1 << 7
        };
        ENUM_CLASS_FLAGS(TextureUsageFlags)

        enum class TextureCreateFlags
        {
            Empty = 0,
            SPARSE_BINDING = 1,
            SPARSE_RESIDENCY = 1 << 1,
            SPARSE_ALIASED =  1 << 2,
            MUTABLE_FORMAT = 1 << 3,
            CUBE_COMPATIBLE = 1 << 4,
            SUBSAMPLED = 1 << 5,
        };
        ENUM_CLASS_FLAGS(TextureCreateFlags)

        enum ImageLayout
        {
              IMAGE_LAYOUT_UNDEFINED = 0,
              IMAGE_LAYOUT_GENERAL = 1,
              IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
              IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
              IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
              IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
              IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
              IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
              IMAGE_LAYOUT_PREINITIALIZED = 8,
              IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL = 1000117000,
              IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL = 1000117001,
              IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002
        };

       
        struct GpuTextureDesc 
        {
            TextureType image_type;
            TextureUsageFlags	usage = (TextureUsageFlags)(1 << 8 - 1);
            TextureCreateFlags flags = TextureCreateFlags::Empty;
            PixelFormat	format = PixelFormat::R8G8B8A8_UNorm;
            std::array<u32, 3> extent;
            u32			mip_levels = 1u;
            u32			array_elements = 1u;
            u32             sample_count = 1u;
            GpuTextureDesc()  = default;
            GpuTextureDesc(PixelFormat format, TextureType image_type, std::array<u32, 3> extent);
            GpuTextureDesc(TextureType image_type,TextureUsageFlags usa, TextureCreateFlags flag, 
                PixelFormat format, std::array<u32, 3> extent,
                u32 mip, u32 array_ele);
            static inline auto	new_1d(PixelFormat format, u32 extent)->GpuTextureDesc
            {
                return GpuTextureDesc(format, TextureType::Tex1d, { extent, 1,1 });
            }
            static inline auto	new_2d(PixelFormat format, std::array<u32, 2> extent)->GpuTextureDesc
            {
                return GpuTextureDesc(format, TextureType::Tex2d, { extent[0], extent[1],1 });
            }
            static inline auto new_3d(PixelFormat format, std::array<u32, 3> extent)->GpuTextureDesc
            {
                return GpuTextureDesc(format, TextureType::Tex3d, extent);
            }
            static inline auto new_cube(PixelFormat format,u32 width)->GpuTextureDesc
            {
                return {
                    TextureType::Cube, 
                    TextureUsageFlags::SAMPLED,
                    TextureCreateFlags::CUBE_COMPATIBLE,
                    format,
                    {width, width,1},
                    1,
                    6
                };

            }
            inline auto with_sample_count()->GpuTextureDesc&
            {
                this->sample_count = sample_count;
                return *this;
            }

            inline auto with_image_type(TextureType image_type)->GpuTextureDesc&
            {
                this->image_type = image_type;
                return *this;
            }

            inline auto with_usage(TextureUsageFlags	usage)->GpuTextureDesc&
            {
                this->usage = usage;
                return *this;
            }

            inline auto with_flag(TextureCreateFlags flag)->GpuTextureDesc&
            {
                this->flags = flag;
                return *this;
            }

            inline auto with_format(PixelFormat format)->GpuTextureDesc&
            {
                this->format = format;
                return *this;
            }
            inline auto with_extent(std::array<u32,3> extent)->GpuTextureDesc&
            {
                this->extent = extent;
                return *this;
            }
  
            inline auto with_mip_levels(uint16 mip_levels)->GpuTextureDesc&
            {
                this->mip_levels = mip_levels;
                return *this;
            }

            inline auto all_mip_levels() -> GpuTextureDesc&
            {
                mip_levels = std::max( std::max(mip_count_1d(extent[0]), mip_count_1d(extent[1])), mip_count_1d(extent[2]));
                return *this;
            }

            inline auto with_array_elements(u32 array_elements)->GpuTextureDesc&
            {
                this->array_elements = array_elements;
                return *this;
            }

            auto div_up_extent(const std::array<u32,3>& div_extent) -> GpuTextureDesc&
            {
                extent[0] = std::max<u32>(1, (extent[0] + div_extent[0] - 1) / div_extent[0]);
                extent[1] = std::max<u32>(1, (extent[1] + div_extent[1] - 1) / div_extent[1]);
                extent[2] = std::max<u32>(1, (extent[2] + div_extent[2] - 1) / div_extent[2]);
                return *this;
            }

            auto div_extent(const std::array<u32, 3>& div_extent) -> GpuTextureDesc&
            {
                extent[0] = std::max<u32>(1,extent[0] / div_extent[0]);
                extent[1] = std::max<u32>(1, extent[1] / div_extent[1]);
                extent[2] = std::max<u32>(1, extent[2] / div_extent[2]);
                return *this;
            }

            auto half_res() const-> GpuTextureDesc
            {
                auto tex_desc = *this;
                return tex_desc.div_up_extent({2, 2, 2});
            }

            auto extent_inv_extent_2d() const-> std::array<float, 4>
            {
                return {
                    (float)extent[0],
                    (float)extent[1],
                    1.0f / extent[0],
                    1.0f / extent[1]
                };
            }

            auto extent_2d() const-> std::array<u32, 2>
            {
                return {
                    extent[0],
                    extent[1]
                };
            }

        };

        struct ImageSubData 
        {
            uint8*	data;
            u32     size;
            u32 row_pitch;
            u32 slice_pitch;
        };

        inline auto operator==(const GpuTextureDesc& s1, const GpuTextureDesc& desc)->bool
        {
            return s1.array_elements == desc.array_elements
                && s1.extent == desc.extent
                && s1.flags == desc.flags
                && s1.format == desc.format
                && s1.image_type == desc.image_type
                && s1.mip_levels == desc.mip_levels
                && s1.usage == desc.usage;
        }

        inline auto operator==(const GpuTextureViewDesc& s1, const GpuTextureViewDesc& s2)-> bool
        {
            return s1.aspect_mask == s2.aspect_mask
                && s1.base_mip_level == s2.base_mip_level
                && s1.format == s2.format
                && s1.level_count == s2.level_count
                && s1.view_type.value_or(TextureType::Tex2d) == s2.view_type.value_or(TextureType::Tex2d);
        }

    }
}

template<>
struct std::hash<diverse::rhi::GpuTextureViewDesc>
{
	std::size_t operator()(diverse::rhi::GpuTextureViewDesc const& s) const noexcept
	{
		auto hash_code = diverse::hash<u32>()(u32(s.view_type.value_or(diverse::rhi::TextureType::Tex2d)));
        diverse::hash_combine(hash_code, u32(s.level_count));
        diverse::hash_combine(hash_code, u32(s.aspect_mask));
        diverse::hash_combine(hash_code, u32(s.base_mip_level));
        diverse::hash_combine(hash_code, u32(s.format));
		return hash_code;
	}
};

template<>
struct std::hash<diverse::rhi::GpuTextureDesc>
{
	std::size_t operator()(diverse::rhi::GpuTextureDesc const& s) const noexcept
	{
		auto hash_code = diverse::hash<u32>()(u32(s.array_elements));
        diverse::hash_combine(hash_code, u32(s.flags));
        diverse::hash_combine(hash_code, u32(s.format));
        diverse::hash_combine(hash_code, u32(s.image_type));
        diverse::hash_combine(hash_code, u32(s.mip_levels));
        diverse::hash_combine(hash_code, u32(s.usage));
        diverse::hash_combine(hash_code, u32(s.extent[0]));
        diverse::hash_combine(hash_code, u32(s.extent[1]));
        diverse::hash_combine(hash_code, u32(s.extent[2]));
		return hash_code;
	}
};


namespace diverse
{
    namespace rhi
    {
        struct GpuTextureView
        {
            virtual ~GpuTextureView();
        };

        struct TextureRegion
        {
            std::array<u32,3>   image_offset;
            std::array<u32,3>   image_extent;
        };

        struct GpuTexture : public GpuResource
        {
            using Desc = GpuTextureDesc;
            // uint64	handle;
            Desc desc;
            // std::unordered_map<GpuTextureViewDesc, VkImageView>	views;
            GpuTexture() { ty = GpuResource::Type::Texture;}
            virtual ~GpuTexture();
            auto export_texture()->std::vector<u8>;
            auto tex_desc() const ->Desc {return desc;}
            virtual auto view(const struct GpuDevice* device, const GpuTextureViewDesc& desc)->std::shared_ptr<GpuTextureView> = 0;
        };

        auto image_access_type_to_usage_flags(AccessType access_type)->TextureUsageFlags;
    }
}

template<>
struct fmt::formatter<diverse::rhi::TextureType> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::TextureType v, format_context& ctx) const -> decltype(ctx.out())
    {
        std::string str;
        switch (v)
        {
        case diverse::rhi::Tex1d:
            str = "Tex1d";
            break;
        case diverse::rhi::Tex1dArray:
            str = "Tex1dArray";
            break;
        case diverse::rhi::Tex2d:
            str = "Tex2d";
            break;
        case diverse::rhi::Tex2dArray:
            str = "Tex2dArray";
            break;
        case diverse::rhi::Tex3d:
            str = "Tex3d";
            break;
        case diverse::rhi::Cube:
            str = "Cube";
            break;
        case diverse::rhi::CubeArray:
            str = "CubeArray";
            break;
        default:
            break;
        }
        return fmt::format_to(ctx.out(), "TextureType: {}", str);
    }
};
template<>
struct fmt::formatter<diverse::rhi::TextureUsageFlags> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::TextureUsageFlags v, format_context& ctx) const -> decltype(ctx.out())
    {
        std::string str;
        switch (v)
        {
        case diverse::rhi::TextureUsageFlags::TRANSFER_SRC:
            str = "TextureUsageFlags::TRANSFER_SRC";
            break;
        case diverse::rhi::TextureUsageFlags::TRANSFER_DST:
            str = "TextureUsageFlags::TRANSFER_DST";
            break;
        case diverse::rhi::TextureUsageFlags::SAMPLED:
            str = "TextureUsageFlags::SAMPLED";
            break;
        case diverse::rhi::TextureUsageFlags::STORAGE:
            str = "TextureUsageFlags::STORAGE";
            break;
        case diverse::rhi::TextureUsageFlags::COLOR_ATTACHMENT:
            str = "TextureUsageFlags::COLOR_ATTACHMENT";
            break;
        case diverse::rhi::TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT:
            str = "TextureUsageFlags::DEPTH_STENCIL_ATTACHMENT";
            break;
        case diverse::rhi::TextureUsageFlags::TRANSIENT_ATTACHMENT:
            str = "TextureUsageFlags::TRANSIENT_ATTACHMENT";
            break;
        case diverse::rhi::TextureUsageFlags::INPUT_ATTACHMENT:
            str = "TextureUsageFlags::INPUT_ATTACHMENT";
            break;
        default:
            break;
        }
        return fmt::format_to(ctx.out(), "{}", str);
    }
};

template<>
struct fmt::formatter<diverse::rhi::TextureCreateFlags> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::TextureCreateFlags v, format_context& ctx) const -> decltype(ctx.out())
    {
        std::string str;
        switch (v)
        {
        case diverse::rhi::TextureCreateFlags::Empty:
            str = "TextureCreateFlags::Empty";
            break;
        case diverse::rhi::TextureCreateFlags::SPARSE_BINDING:
            str = "TextureCreateFlags::SPARSE_BINDING";
            break;
        case diverse::rhi::TextureCreateFlags::SPARSE_RESIDENCY:
            str = "TextureCreateFlags::SPARSE_RESIDENCY";
            break;
        case diverse::rhi::TextureCreateFlags::SPARSE_ALIASED:
            str = "TextureCreateFlags::SPARSE_ALIASED";
            break;
        case diverse::rhi::TextureCreateFlags::MUTABLE_FORMAT:
            str = "TextureCreateFlags::MUTABLE_FORMAT";
            break;
        case diverse::rhi::TextureCreateFlags::CUBE_COMPATIBLE:
            str = "TextureCreateFlags::CUBE_COMPATIBLE";
            break;
        case diverse::rhi::TextureCreateFlags::SUBSAMPLED:
            str = "TextureCreateFlags::SUBSAMPLED";
            break;
        default:
            break;
        }
        return fmt::format_to(ctx.out(), "{}", str);
    }
};

template<>
struct fmt::formatter<diverse::rhi::GpuTextureDesc> : fmt::formatter<std::string>
{
    auto format(diverse::rhi::GpuTextureDesc v, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "GpuTextureDesc image_type: {}, usage: {}, flags :{}, format: {}, externt:[{},{},{}], mip_levels:{}, array_elements:{}", \
            v.image_type, v.usage, v.flags, v.format, v.extent[0],v.extent[1],v.extent[2], v.mip_levels, v.array_elements);
    }
};