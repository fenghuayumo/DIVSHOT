#include "texture_importer.h"
#include "asset_id.h"
#include "core/ds_log.h"
#include <algorithm>
#include <cmath>

namespace diverse
{
    namespace
    {
        auto log2_int(const std::array<u32, 2>& extent) -> u32
        {
            return (u32)std::max(std::ceil(std::log2(extent[0])), std::ceil(std::log2(extent[1])));
        }

        auto build_mips(const image_io::RawImage& image, bool generate_mips) -> std::vector<MipData>
        {
            std::vector<MipData> mips;
            auto push_mip = [&](const image_io::RawImage& mip) {
                MipData data;
                data.width = mip.dimensions[0];
                data.height = mip.dimensions[1];
                data.depth = 1;
                data.data = mip.data;
                mips.push_back(std::move(data));
            };

            push_mip(image);
            if (!generate_mips)
                return mips;

            const auto can_resize = [](PixelFormat format) {
                switch (format)
                {
                case PixelFormat::R8G8B8A8_UNorm:
                case PixelFormat::R8G8B8A8_UNorm_sRGB:
                case PixelFormat::R16G16B16A16_Float:
                case PixelFormat::R32G32B32A32_Float:
                    return true;
                default:
                    return false;
                }
            };
            if (!can_resize(image.format))
            {
                DS_LOG_WARN("Skipping CPU mip generation for unsupported texture format {}", (u32)image.format);
                return mips;
            }

            auto downsample = [](const image_io::RawImage& src) {
                return src.resize(std::max(1, (int)src.width() / 2), std::max(1, (int)src.height() / 2));
            };

            auto next = downsample(image);
            auto levels = log2_int(image.dimensions);
            for (u32 level = 1; level < levels; ++level)
            {
                push_mip(next);
                if (next.width() <= 1 && next.height() <= 1)
                    break;
                next = downsample(next);
            }
            return mips;
        }

        std::shared_ptr<TextureAsset> import_internal(const image_io::RawImage& image, const TextureImportSettings& settings, const std::filesystem::path& source_path)
        {
            if (image.dimensions[0] == 0 || image.dimensions[1] == 0 || image.data.empty())
                return nullptr;

            auto texture = std::make_shared<TextureAsset>();
            texture->id = GenerateAssetId();
            texture->source_path = source_path;
            texture->settings = settings;
            texture->format = image.format;
            if (settings.srgb && texture->format == PixelFormat::R8G8B8A8_UNorm)
                texture->format = PixelFormat::R8G8B8A8_UNorm_sRGB;
            texture->extent = { image.dimensions[0], image.dimensions[1], 1 };
            texture->mips = build_mips(image, settings.generate_mips);
            texture->cpu_memory_size = texture->calculate_memory_size();
            texture->version = 0;
            return texture;
        }
    }

    std::shared_ptr<TextureAsset> import_texture_from_raw(const image_io::RawImage& image, const TextureImportSettings& settings)
    {
        return import_internal(image, settings, {});
    }

    std::shared_ptr<TextureAsset> import_texture_from_path(const std::filesystem::path& path, const TextureImportSettings& settings)
    {
        try
        {
            auto image = image_io::load_image(path);
            return import_internal(image, settings, path);
        }
        catch (const std::exception& e)
        {
            DS_LOG_ERROR("Failed to import texture '{}': {}", path.string(), e.what());
            return nullptr;
        }
    }

    std::shared_ptr<TextureAsset> create_white_texture_asset()
    {
        return import_texture_from_raw(image_io::RawImage{
            PixelFormat::R8G8B8A8_UNorm, { 1, 1 }, { 255, 255, 255, 255 } });
    }

    std::shared_ptr<TextureAsset> create_black_texture_asset()
    {
        return import_texture_from_raw(image_io::RawImage{
            PixelFormat::R8G8B8A8_UNorm, { 1, 1 }, { 0, 0, 0, 255 } });
    }

    std::shared_ptr<TextureAsset> create_normal_texture_asset()
    {
        return import_texture_from_raw(image_io::RawImage{
            PixelFormat::R8G8B8A8_UNorm, { 1, 1 }, { 127, 127, 255, 255 } });
    }

    std::shared_ptr<TextureAsset> create_rgb_texture_asset(const std::array<u8, 4>& rgba)
    {
        return import_texture_from_raw(image_io::RawImage{
            PixelFormat::R8G8B8A8_UNorm, { 1, 1 }, { rgba[0], rgba[1], rgba[2], rgba[3] } });
    }
}
