
#pragma once

#include <vector>
#include <string>
#include <array>
#include <any>
#include <optional>
#include <glm/glm.hpp>
#include "backend/drs_rhi/pixel_format.h"
#include "core/base_type.h"
#include "core/half_float.h"
#include "utility/file_utils.h"
#include "assets/asset.h"
namespace diverse
{
    namespace rhi
    {
        struct GpuTexture;
    }
    namespace asset
    {
        struct ImageSource
        {
            enum {
                File,
                Memory
            }ty;
            // std::any value;

            std::string file_path;
            std::vector<u8>   data;
        };
        
        enum class TexGamma
        {
            Linear,
            Srgb
        };

        enum class TexCompressionMode
        {
            None,
            Rgba,
            Rg
        };

        enum class BcMode
        {
            Bc5,
            Bc7
        };

        inline auto block_bytes(BcMode mode) -> u32
        {
            switch ( mode )
            {
            case BcMode::Bc5:
                return 16;
            case BcMode::Bc7:
                return 16;
            default:
                break;
            }
            return 0;
        }

        inline bool supports_alpha(TexCompressionMode mode)
        {
            switch (mode)
            {
            case diverse::asset::TexCompressionMode::None:
                return true;
            case diverse::asset::TexCompressionMode::Rgba:
                return true;
            case diverse::asset::TexCompressionMode::Rg:
                return false;
            default:
                return false;
            }
            return true;
        }

        struct TexParams
        {
            TexGamma    gamma = TexGamma::Linear;
            bool        use_mips = false;
            TexCompressionMode  compression = TexCompressionMode::None;
            std::optional<std::array<u32,4>>   channel_swizzle;
        };

        struct PixelValue
        {
            enum {
                RGBA8,
                DDS,
                Float,
                HalfFloat
            }ty;
            std::any    value;
        };

        struct RawImage
        {
            PixelFormat format;
            std::array<u32, 2> dimensions;
            std::vector<u8> data;

            void put(u32 x, u32 y, const std::array<u8, 4>& rgba)
            {
                auto offset = (y * dimensions[0] + x) * 4;
                // data[offset]
                memcpy(&data[offset], rgba.data(), sizeof(u8) * 4);
            }

            void put(u32 x, u32 y, const std::array<f32, 4>& rgba)
            {
                auto offset = (y * dimensions[0] + x) * 4 * 4;
                // data[offset]
                memcpy(&data[offset], rgba.data(), sizeof(f32) * 4);
            }
            void put_f16(u32 x, u32 y, const std::array<f32, 4>& rgba)
            {
                auto offset = (y * dimensions[0] + x) * 4 * 2;
                auto f16_ptr = reinterpret_cast<f16*>(data.data());
                for (auto i = 0; i < 4; i++) {
                    f16_ptr[offset+i] = rgba[i];
                }
            }
            std::array<f32, 4> get_f32(u32 x, u32 y) {
                auto offset = (y * dimensions[0] + x) * 4;
                float* data_ptr = reinterpret_cast<float*>(data.data());
                return {data_ptr[offset],data_ptr[offset+1],data_ptr[offset+2],data_ptr[offset+3] };
            }
            std::array<f16, 4> get_f16(u32 x, u32 y) {
                auto offset = (y * dimensions[0] + x) * 4;
                f16* data_ptr = reinterpret_cast<f16*>(data.data());
                return {data_ptr[offset],data_ptr[offset+1], data_ptr[offset+2], data_ptr[offset+3]};
            }
            std::array<u8, 4> get_u8(u32 x, u32 y) {
                auto offset = (y * dimensions[0] + x) * 4;
                u8* data_ptr = reinterpret_cast<u8*>(data.data());
                return { data_ptr[offset],data_ptr[offset + 1], data_ptr[offset + 2], data_ptr[offset + 3] };
            }
            auto resize(int w,int h) const-> RawImage;
            auto width() const ->u32;
            auto height() const ->u32;
            auto convert(PixelFormat format) const ->RawImage;
        };

        struct Texture : public Asset
        {
            std::array<u32, 3> extent;
            std::vector<std::vector<u8>> mips;

            TexParams params;
            std::shared_ptr<rhi::GpuTexture> gpu_texture;
            std::string file_path;
        public:
            Texture(const RawImage& img);
            Texture(const ImageSource& source);
            Texture(u32 width, u32 height,const std::vector<u8>& data, PixelFormat format = PixelFormat::B8G8R8A8_UNorm);
            Texture(const std::filesystem::path& path);
            Texture() {}
            auto    get_file_path()->std::string {return file_path;}
            static auto create_white_texture() -> Texture;
            static auto create_black_texture() -> Texture;
            static auto create_rgb_texture(const std::array<u8,4>& rgba) -> Texture;
        public:
            AssetType get_asset_type() const override { return AssetType::Texture; }
            void init_from_raw_image(const RawImage& img, const TexParams& param);
            void init_from_path(const std::filesystem::path& path);
        protected:
            auto    upload_2_gpu(PixelFormat format) -> void;
        };

        auto load_float_image(const std::filesystem::path& path)-> RawImage;
        auto load_hdr(const std::filesystem::path& file_path)-> RawImage;
        auto load_exr(const std::filesystem::path& file_path)-> RawImage;
        auto load_image(const std::filesystem::path& file_path)->RawImage;
    }
}