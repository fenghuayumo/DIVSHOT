#pragma once

#include "backend/drs_rhi/pixel_format.h"
#include "core/base_type.h"
#include "core/half_float.h"
#include <array>
#include <filesystem>
#include <vector>

namespace diverse
{
    namespace image_io
    {
        struct RawImage
        {
            PixelFormat format;
            std::array<u32, 2> dimensions;
            std::vector<u8> data;

            void put(u32 x, u32 y, const std::array<u8, 4>& rgba);
            void put(u32 x, u32 y, const std::array<f32, 4>& rgba);
            void put_f16(u32 x, u32 y, const std::array<f32, 4>& rgba);
            std::array<f32, 4> get_f32(u32 x, u32 y);
            std::array<f16, 4> get_f16(u32 x, u32 y);
            std::array<u8, 4> get_u8(u32 x, u32 y);
            auto resize(int w, int h) const -> RawImage;
            auto width() const -> u32;
            auto height() const -> u32;
            auto convert(PixelFormat format) const -> RawImage;
        };

        auto load_float_image(const std::filesystem::path& path) -> RawImage;
        auto load_hdr(const std::filesystem::path& file_path) -> RawImage;
        auto load_exr(const std::filesystem::path& file_path) -> RawImage;
        auto load_image(const std::filesystem::path& file_path) -> RawImage;
    }

    // Legacy namespace alias used by environment / font paths during migration.
    namespace asset
    {
        using RawImage = image_io::RawImage;
        using image_io::load_exr;
        using image_io::load_float_image;
        using image_io::load_hdr;
        using image_io::load_image;
    }
}
