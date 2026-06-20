#include "image_io.h"
#include <stb/image_utils.h>
#define TINYDDSLOADER_IMPLEMENTATION
#include <tinydds/tinyddsloader.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
#include "core/ds_log.h"
#include <algorithm>
#include <cstring>
#include <glm/glm.hpp>

namespace diverse
{
    namespace image_io
    {
        namespace
        {
            auto divide_up_by_multiple(u32 val, u32 align) -> u32
            {
                u32 mask = align - 1;
                return (val + mask) / align;
            }

            auto is_block_compressed(PixelFormat format) -> bool
            {
                switch (format)
                {
                case PixelFormat::BC1_UNorm:
                case PixelFormat::BC1_UNorm_sRGB:
                case PixelFormat::BC3_UNorm:
                case PixelFormat::BC3_UNorm_sRGB:
                case PixelFormat::BC7_UNorm:
                case PixelFormat::BC7_UNorm_sRGB:
                    return true;
                default:
                    return false;
                }
            }

            auto pixel_format_from_dds(tinyddsloader::DDSFile::DXGIFormat dds_format) -> PixelFormat
            {
                using DXGIFormat = tinyddsloader::DDSFile::DXGIFormat;
                switch (dds_format)
                {
                case DXGIFormat::BC1_UNorm: return PixelFormat::BC1_UNorm;
                case DXGIFormat::BC1_UNorm_SRGB: return PixelFormat::BC1_UNorm_sRGB;
                case DXGIFormat::BC3_UNorm: return PixelFormat::BC3_UNorm;
                case DXGIFormat::BC3_UNorm_SRGB: return PixelFormat::BC3_UNorm_sRGB;
                case DXGIFormat::BC7_UNorm: return PixelFormat::BC7_UNorm;
                case DXGIFormat::BC7_UNorm_SRGB: return PixelFormat::BC7_UNorm_sRGB;
                case DXGIFormat::R8G8B8A8_UNorm: return PixelFormat::R8G8B8A8_UNorm;
                case DXGIFormat::R8G8B8A8_UNorm_SRGB: return PixelFormat::R8G8B8A8_UNorm_sRGB;
                default:
                    throw std::runtime_error{ fmt::format("unsupported DDS format ({})", static_cast<uint32_t>(dds_format)) };
                }
            }

            auto load_dds(const std::filesystem::path& path) -> RawImage
            {
                tinyddsloader::DDSFile dds;
                if (tinyddsloader::Result::Success != dds.Load(path.string().c_str()))
                    throw std::runtime_error{ fmt::format("failed to load DDS '{}'", path.string()) };

                const auto* image_data = dds.GetImageData(0, 0);
                if (!image_data || !image_data->m_mem || image_data->m_memSlicePitch == 0)
                    throw std::runtime_error{ fmt::format("DDS '{}' contains no mip data", path.string()) };

                RawImage image;
                image.format = pixel_format_from_dds(dds.GetFormat());
                image.dimensions = { dds.GetWidth(), dds.GetHeight() };
                image.data.resize(image_data->m_memSlicePitch);
                std::memcpy(image.data.data(), image_data->m_mem, image_data->m_memSlicePitch);
                return image;
            }
        }

        void RawImage::put(u32 x, u32 y, const std::array<u8, 4>& rgba)
        {
            auto offset = (y * dimensions[0] + x) * 4;
            std::memcpy(&data[offset], rgba.data(), sizeof(u8) * 4);
        }

        void RawImage::put(u32 x, u32 y, const std::array<f32, 4>& rgba)
        {
            auto offset = (y * dimensions[0] + x) * 4 * 4;
            std::memcpy(&data[offset], rgba.data(), sizeof(f32) * 4);
        }

        void RawImage::put_f16(u32 x, u32 y, const std::array<f32, 4>& rgba)
        {
            auto offset = (y * dimensions[0] + x) * 4 * 2;
            auto f16_ptr = reinterpret_cast<f16*>(data.data());
            for (auto i = 0; i < 4; i++)
                f16_ptr[offset + i] = rgba[i];
        }

        std::array<f32, 4> RawImage::get_f32(u32 x, u32 y)
        {
            auto offset = (y * dimensions[0] + x) * 4;
            float* data_ptr = reinterpret_cast<float*>(data.data());
            return { data_ptr[offset], data_ptr[offset + 1], data_ptr[offset + 2], data_ptr[offset + 3] };
        }

        std::array<f16, 4> RawImage::get_f16(u32 x, u32 y)
        {
            auto offset = (y * dimensions[0] + x) * 4;
            f16* data_ptr = reinterpret_cast<f16*>(data.data());
            return { data_ptr[offset], data_ptr[offset + 1], data_ptr[offset + 2], data_ptr[offset + 3] };
        }

        std::array<u8, 4> RawImage::get_u8(u32 x, u32 y)
        {
            auto offset = (y * dimensions[0] + x) * 4;
            u8* data_ptr = reinterpret_cast<u8*>(data.data());
            return { data_ptr[offset], data_ptr[offset + 1], data_ptr[offset + 2], data_ptr[offset + 3] };
        }

        auto load_float_image(const std::filesystem::path& path) -> RawImage
        {
            auto ext = path.extension();
            if (ext == ".hdr")
                return load_hdr(path);
            if (ext == ".exr")
                return load_exr(path);
            throw std::runtime_error{ fmt::format("unknown image extension '{}'", path.extension()) };
        }

        auto load_hdr(const std::filesystem::path& path) -> RawImage
        {
            RawImage rgbf;
            rgbf.format = PixelFormat::R32G32B32A32_Float;
            int comp;
            auto fData = load_stbi_float(path, (int*)&rgbf.dimensions[0], (int*)&rgbf.dimensions[1], &comp, 4);
            if (!fData)
                throw std::runtime_error{ fmt::format("load_hdr: {} error", path.string()) };
            rgbf.data.resize(rgbf.dimensions[0] * rgbf.dimensions[1] * 4 * sizeof(f32));
            std::memcpy(rgbf.data.data(), fData, rgbf.data.size());
            free(fData);
            return rgbf;
        }

        auto load_exr(const std::filesystem::path& path) -> RawImage
        {
            RawImage rgbf;
            rgbf.format = PixelFormat::R32G32B32A32_Float;
            const char* err = NULL;
            float* fData = nullptr;
            int ret = LoadEXR(&fData, (int*)&rgbf.dimensions[0], (int*)&rgbf.dimensions[1], path.string().c_str(), &err);
            if (ret != TINYEXR_SUCCESS)
                throw std::runtime_error{ fmt::format("load_exr: {} '{}'", path.string(), err) };
            rgbf.data.resize(rgbf.dimensions[0] * rgbf.dimensions[1] * 4 * sizeof(f32));
            std::memcpy(rgbf.data.data(), fData, rgbf.data.size());
            free(fData);
            return rgbf;
        }

        auto load_image(const std::filesystem::path& path) -> RawImage
        {
            if (!std::filesystem::exists(path))
            {
                DS_LOG_ERROR("file {} not found", path.string());
                throw std::runtime_error{ fmt::format(" file {} not found ", path.extension().string()) };
            }
            auto ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".dds")
                return load_dds(path);
            if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga")
            {
                int w, h, comp;
                u8* data = load_stbi(path, &w, &h, &comp, 4);
                std::vector<u8> dst(w * h * 4);
                std::memcpy(dst.data(), data, dst.size());
                free(data);
                return RawImage{ PixelFormat::R8G8B8A8_UNorm, { (u32)w, (u32)h }, std::move(dst) };
            }
            if (ext == ".hdr" || ext == ".exr")
                return load_float_image(path);
            DS_LOG_ERROR("unknown image extension '{}'", path.extension().string());
            throw std::runtime_error{ fmt::format(" unknown image extension {} ", path.extension().string()) };
        }

        auto RawImage::resize(int w, int h) const -> RawImage
        {
            if (w <= 0 || h <= 0 || data.empty())
                return {};

            switch (format)
            {
            case PixelFormat::R8G8B8A8_UNorm:
            case PixelFormat::R8G8B8A8_UNorm_sRGB:
            {
                std::vector<u8> dst(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
                stb_resize(data.data(), (int)dimensions[0], (int)dimensions[1], dst.data(), w, h, STB_RGBA);
                return RawImage{ format, { (u32)w, (u32)h }, std::move(dst) };
            }
            case PixelFormat::R32G32B32A32_Float:
            {
                std::vector<u8> dst(static_cast<size_t>(w) * static_cast<size_t>(h) * 4 * sizeof(f32));
                stb_resize(
                    reinterpret_cast<const f32*>(data.data()),
                    (int)dimensions[0],
                    (int)dimensions[1],
                    reinterpret_cast<f32*>(dst.data()),
                    w,
                    h,
                    STB_RGBA);
                return RawImage{ format, { (u32)w, (u32)h }, std::move(dst) };
            }
            case PixelFormat::R16G16B16A16_Float:
                return convert(PixelFormat::R32G32B32A32_Float).resize(w, h).convert(format);
            default:
                DS_LOG_WARN("RawImage::resize unsupported pixel format {}; returning original image", (u32)format);
                return *this;
            }
        }

        auto RawImage::width() const -> u32 { return dimensions[0]; }
        auto RawImage::height() const -> u32 { return dimensions[1]; }

        auto RawImage::convert(PixelFormat type) const -> RawImage
        {
            if (format == PixelFormat::R32G32B32A32_Float)
            {
                if (type == PixelFormat::R16G16B16A16_Float)
                {
                    RawImage img = { PixelFormat::R16G16B16A16_Float, dimensions };
                    img.data.resize(data.size() / 2);
                    auto f32_ptr = reinterpret_cast<const f32*>(data.data());
                    auto f16_ptr = reinterpret_cast<f16*>(img.data.data());
                    for (size_t i = 0; i < data.size() / 4; i++)
                        f16_ptr[i] = f32_ptr[i];
                    return img;
                }
                if (type == PixelFormat::R8G8B8A8_UNorm)
                {
                    RawImage img = { PixelFormat::R8G8B8A8_UNorm, dimensions };
                    img.data.resize(data.size() / 4);
                    auto f32_ptr = reinterpret_cast<const f32*>(data.data());
                    auto u8_ptr = reinterpret_cast<u8*>(img.data.data());
                    for (size_t i = 0; i < data.size() / 4; i++)
                        u8_ptr[i] = (u8)glm::clamp(f32_ptr[i] * 255.0f, 0.0f, 255.0f);
                    return img;
                }
            }
            else if (format == PixelFormat::R8G8B8A8_UNorm)
            {
                if (type == PixelFormat::R16G16B16A16_Float)
                {
                    RawImage img = { PixelFormat::R16G16B16A16_Float, dimensions };
                    img.data.resize(data.size() * 2);
                    auto u8_ptr = reinterpret_cast<const u8*>(data.data());
                    auto f16_ptr = reinterpret_cast<f16*>(img.data.data());
                    for (size_t i = 0; i < data.size(); i++)
                        f16_ptr[i] = u8_ptr[i] / 255.0f;
                    return img;
                }
                if (type == PixelFormat::R32G32B32A32_Float)
                {
                    RawImage img = { PixelFormat::R32G32B32A32_Float, dimensions };
                    img.data.resize(data.size() * 4);
                    auto u8_ptr = reinterpret_cast<const u8*>(data.data());
                    auto f32_ptr = reinterpret_cast<f32*>(img.data.data());
                    for (size_t i = 0; i < data.size(); i++)
                        f32_ptr[i] = u8_ptr[i] / 255.0f;
                    return img;
                }
            }
            else if (format == PixelFormat::R16G16B16A16_Float)
            {
                if (type == PixelFormat::R8G8B8A8_UNorm)
                {
                    RawImage img = { PixelFormat::R8G8B8A8_UNorm, dimensions };
                    img.data.resize(data.size() / 2);
                    auto f16_ptr = reinterpret_cast<const f16*>(data.data());
                    auto u8_ptr = reinterpret_cast<u8*>(img.data.data());
                    for (size_t i = 0; i < data.size() / 2; i++)
                        u8_ptr[i] = (u8)glm::clamp(f16_ptr[i] * 255.0f, 0.0f, 255.0f);
                    return img;
                }
                if (type == PixelFormat::R32G32B32A32_Float)
                {
                    RawImage img = { PixelFormat::R32G32B32A32_Float, dimensions };
                    img.data.resize(data.size() * 2);
                    auto f16_ptr = reinterpret_cast<const f16*>(data.data());
                    auto f32_ptr = reinterpret_cast<f32*>(img.data.data());
                    for (size_t i = 0; i < data.size() / 2; i++)
                        f32_ptr[i] = f16_ptr[i].to_f32();
                    return img;
                }
            }
            return *this;
        }
    }
}
