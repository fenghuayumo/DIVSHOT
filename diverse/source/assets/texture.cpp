
#include "texture.h"
#include <stb/image_utils.h>
#define TINYDDSLOADER_IMPLEMENTATION
#include <tinydds/tinyddsloader.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
#include "core/ds_log.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_device.h"
namespace diverse
{
    namespace asset
    {
          auto divide_up_by_multiple(u32 val,u32 align) -> u32 
        {
            u32 mask = align - 1;
            return (val + mask) / align;
        }

        auto log2_int(const std::array<u32, 2>& extent) -> u32
        {
            return (u32)std::max(std::ceil(std::log2(extent[0])), std::ceil(std::log2(extent[1])));
        }

        auto load_float_image(const std::filesystem::path& path)-> RawImage
        {
            auto ext = path.extension();
            if (ext == ".hdr")
                return load_hdr(path.string());
            else if( ext == ".exr")
                return load_exr(path.string());
           throw std::runtime_error{fmt::format("unknown image extension '{}'", path.extension())};
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

        auto block_compressed_row_pitch(PixelFormat format, u32 width) -> u32
        {
            switch (format)
            {
            case PixelFormat::BC1_UNorm:
            case PixelFormat::BC1_UNorm_sRGB:
            case PixelFormat::BC3_UNorm:
            case PixelFormat::BC3_UNorm_sRGB:
            case PixelFormat::BC7_UNorm:
            case PixelFormat::BC7_UNorm_sRGB:
                return divide_up_by_multiple(width, 4) * 16;
            default:
                return width * 4;
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


        auto load_hdr(const std::filesystem::path& path)-> RawImage
        {
            RawImage rgbf;
            rgbf.format = PixelFormat::R32G32B32A32_Float;
            int comp;
            auto fData = load_stbi_float(path, (int*)&rgbf.dimensions[0], (int*)&rgbf.dimensions[1],&comp, 4);
            if (!fData) {
                throw std::runtime_error{ fmt::format("load_hdr: {} error", path.string()) };
            }
            rgbf.data.resize(rgbf.dimensions[0] * rgbf.dimensions[1] * 4 * sizeof(f32));
            memcpy(rgbf.data.data(), fData, rgbf.data.size());
            free(fData);
            return rgbf;
        }

        auto load_exr(const std::filesystem::path& path)-> RawImage
        {
            RawImage rgbf;
            rgbf.format = PixelFormat::R32G32B32A32_Float;
            const char* err = NULL;
            float* fData = nullptr;
            int ret = LoadEXR(&fData, (int*)&rgbf.dimensions[0], (int*)&rgbf.dimensions[1], path.string().c_str(), &err);
            if (ret != TINYEXR_SUCCESS) {
                throw std::runtime_error{ fmt::format("load_exr: {} '{}'", path.string(),err) };
            }
            rgbf.data.resize(rgbf.dimensions[0] * rgbf.dimensions[1] * 4 * sizeof(f32));
            memcpy(rgbf.data.data(), fData, rgbf.data.size());
            free(fData);
            return rgbf;
        }

        auto load_image(const std::filesystem::path& path)->RawImage
        {
            if (!std::filesystem::exists(path)) {
                DS_LOG_ERROR("file {} not found", path.string());
                throw std::runtime_error{ fmt::format(" file {} not found ", path.extension().string()) };
            }
            auto ext = path.extension().string();
            //transform ext to lower
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if(ext == ".dds"){
                return load_dds(path);
            }
            else if(ext == ".png" || ".jpg" == ext || ".bmp" == ext || ext == ".tga") {
                
                int w,h,comp;
                u8* data = load_stbi(path, &w, &h, &comp, 4);

                std::vector<u8> dst(w * h * 4);
                memcpy(dst.data(), data, dst.size());
                free(data);

                return RawImage{ PixelFormat::R8G8B8A8_UNorm, {(u32)w,(u32)h}, std::move(dst)};
            }
            else if (ext == ".hdr" || ext == ".exr") {
				return load_float_image(path);
			}
            else {
				DS_LOG_ERROR("unknown image extension '{}'", path.extension().string());
				throw std::runtime_error{ fmt::format(" unknown image extension {} ", path.extension().string()) };
			}
            throw std::runtime_error{fmt::format(" unknown image extension {} ", path.extension().string())};
        }

        auto RawImage::resize(int w, int h) const -> RawImage
        {
            std::vector<u8> dst(w * h * get_pixel_bytes(format));
            stb_resize((float*)data.data(), (int)dimensions[0], (int)dimensions[1], (float*)dst.data(), w, h, STB_RGBA);
            return RawImage{ format, {(u32)w,(u32)h}, std::move(dst) };
            return *this;
        }

        auto RawImage::width() const -> u32
        {
            return dimensions[0];
        }

        auto RawImage::height() const -> u32
        {
            return dimensions[1];
        }

        auto RawImage::convert(PixelFormat type) const -> RawImage
        {
            if (format == PixelFormat::R32G32B32A32_Float) {
                if (type == PixelFormat::R16G16B16A16_Float) {
                    RawImage    img = { PixelFormat::R16G16B16A16_Float, dimensions};
                    img.data.resize(this->data.size() / 2);
                    auto f32_ptr = reinterpret_cast<const f32*>(data.data());
                    auto f16_ptr = reinterpret_cast<f16*>(img.data.data());
                    for (auto i = 0; i < data.size() / 4; i++) {
                        f16_ptr[i] = f32_ptr[i];
                    }
                    return img;
                }
                else if (type == PixelFormat::R8G8B8A8_UNorm) {
                    RawImage    img = { PixelFormat::R8G8B8A8_UNorm, dimensions};
                    img.data.resize(this->data.size() / 4);
                    auto f32_ptr = reinterpret_cast<const f32*>(data.data());
                    auto u8_ptr = reinterpret_cast<u8*>(img.data.data());
                    for (auto i = 0; i < data.size() / 4; i++) {
                        u8_ptr[i] = (u8)glm::clamp(f32_ptr[i] * 255.0f,0.0f,255.0f);
                    }
                    return img;
                }
            }
            else if (format == PixelFormat::R8G8B8A8_UNorm) {
                if (type == PixelFormat::R16G16B16A16_Float) {
                    RawImage    img = { PixelFormat::R16G16B16A16_Float, dimensions};
                    img.data.resize(this->data.size() * 2);
                    auto u8_ptr = reinterpret_cast<const u8*>(data.data());
                    auto f16_ptr = reinterpret_cast<f16*>(img.data.data());
                    for (auto i = 0; i < data.size(); i++) {
                        f16_ptr[i] = u8_ptr[i] / 255.0f;
                    }
                    return img;
                }
                else if (type == PixelFormat::R32G32B32A32_Float) {
                    RawImage    img = { PixelFormat::R32G32B32A32_Float, dimensions};
                    img.data.resize(this->data.size() * 4);
                    auto u8_ptr = reinterpret_cast<const u8*>(data.data());
                    auto f32_ptr = reinterpret_cast<f32*>(img.data.data());
                    for (auto i = 0; i < data.size(); i++) {
                        f32_ptr[i] = u8_ptr[i] / 255.0f;
                    }
                    return img;
                }
            }
            else if (format == PixelFormat::R16G16B16A16_Float) {
                if (type == PixelFormat::R8G8B8A8_UNorm) {
                    RawImage    img = { PixelFormat::R8G8B8A8_UNorm, dimensions };
                    img.data.resize(this->data.size() / 2);
                    auto f16_ptr = reinterpret_cast<const f16*>(data.data());
                    auto u8_ptr = reinterpret_cast<u8*>(img.data.data());
                    for (auto i = 0; i < data.size() / 2; i++) {
                        u8_ptr[i] = (u8)glm::clamp(f16_ptr[i] * 255.0f, 0.0f, 255.0f);
                    }
                    return img;
                }
                else if (type == PixelFormat::R32G32B32A32_Float) {
                    RawImage    img = { PixelFormat::R32G32B32A32_Float, dimensions };
                    img.data.resize(this->data.size() * 2);
                    auto f16_ptr = reinterpret_cast<const f16*>(data.data());
                    auto f32_ptr = reinterpret_cast<f32*>(img.data.data());
                    for (auto i = 0; i < data.size() / 2; i++) {
                       f32_ptr[i] = f16_ptr[i].to_f32();
                    }
                    return img;
                }
            }
            return *this;
        }

        Texture::Texture(const RawImage& img)
        {
            init_from_raw_image(img, TexParams{});
        }

        Texture::Texture(const ImageSource& source)
            : Texture(load_image(source.file_path))
        {
        }
        Texture::Texture(u32 width, u32 height, const std::vector<u8>& data, PixelFormat format)
        {
            init_from_raw_image(RawImage{ format, {width,height}, std::move(data) }, TexParams{});
        }

        Texture::Texture(const std::filesystem::path& path)
        : Texture(load_image(path))
        {
            file_path = path.string();
        }

        auto Texture::upload_2_gpu(PixelFormat format, rhi::GpuDevice* device)->void
        {
            if ( !is_flag_set(AssetFlag::UploadedGpu)) {
                if (!device)
                    return;
                std::lock_guard lock(upload_mutex);
                // Double-check after acquiring the lock — another thread may have completed the upload
                if (is_flag_set(AssetFlag::UploadedGpu))
                    return;
                auto desc = rhi::GpuTextureDesc::new_2d(format, { extent[0], extent[1] })
                    .with_usage(rhi::TextureUsageFlags::SAMPLED |
                        rhi::TextureUsageFlags::TRANSFER_DST |
                        rhi::TextureUsageFlags::TRANSFER_SRC)
                    .with_mip_levels(mips.size()); //mips

                std::vector<rhi::ImageSubData> initial_data;
                for (auto mip_level = 0; mip_level < mips.size(); mip_level++)
                {
                    auto& mip = mips[mip_level];
                    const u32 mip_width = std::max<u32>(1u, desc.extent[0] >> mip_level);
                    auto row_pitch = is_block_compressed(format)
                        ? block_compressed_row_pitch(format, mip_width)
                        : mip_width * 4;

                    initial_data.emplace_back(rhi::ImageSubData{ mip.data(), (u32)mip.size(), row_pitch, 0 });
                }
                gpu_texture = device->create_texture(desc, initial_data);
                if (gpu_texture)
                    set_flag(AssetFlag::UploadedGpu);
            }
        }

        auto Texture::create_white_texture()->Texture
        {
			std::vector<u8> white(4, 255);
			return Texture(RawImage{ PixelFormat::R8G8B8A8_UNorm, {1,1}, std::move(white)});
		}
        
        auto Texture::create_black_texture()->Texture
        {
            std::vector<u8> black = {0,0,0,255};
            return Texture(RawImage{ PixelFormat::R8G8B8A8_UNorm, {1,1}, std::move(black) });
        }

        auto Texture::create_rgb_texture(const std::array<u8, 4>& rgba)->Texture
        {
            return Texture(RawImage{ PixelFormat::R8G8B8A8_UNorm, {1,1}, {rgba[0],rgba[1],rgba[2],rgba[3]} });
        }
        
        void Texture::init_from_raw_image(const RawImage& img, const TexParams& param)
        {
            set_flag(AssetFlag::Loaded, false);
            set_flag(AssetFlag::UploadedGpu, false);
            set_flag(AssetFlag::Invalid, false);
            if (img.dimensions[0] == 0 || img.dimensions[1] == 0 || img.data.empty())
            {
                DS_LOG_ERROR("Texture initialisation failed: empty image data");
                set_flag(AssetFlag::Invalid);
                return;
            }

            if (is_block_compressed(img.format))
            {
                format = img.format;
                extent = { img.dimensions[0], img.dimensions[1], 1 };
                mips.clear();
                mips.push_back(img.data);
                upload_2_gpu(format);
                if (gpu_texture)
                    set_flag(AssetFlag::UploadedGpu);
                set_flag(AssetFlag::Loaded);
                return;
            }

            auto rgb_img = img;
            extent = { img.dimensions[0], img.dimensions[1], 1 };
            params = param;
            auto should_compress = false;/*params.compression != TexCompressionMode::None
                       && image.width() >= 4
                       && image.height() >= 4;*/
            constexpr u32 MAX_SIZE = 4096;
            format = PixelFormat::R8G8B8A8_UNorm;
            if (img.dimensions[0] > MAX_SIZE || img.dimensions[1] > MAX_SIZE)
            {
                extent = { std::min(MAX_SIZE, img.dimensions[0]), std::min(MAX_SIZE, img.dimensions[1]), 1 };
                rgb_img = img.resize(std::min(MAX_SIZE, img.dimensions[0]), std::min(MAX_SIZE, img.dimensions[1]));
            }
            if (img.format == PixelFormat::R8G8B8A8_UNorm)
            {
                format = params.gamma == TexGamma::Linear ? PixelFormat::R8G8B8A8_UNorm : PixelFormat::R8G8B8A8_UNorm_sRGB;
            }
            else
                format = img.format;
 

            auto compress = [&](RawImage& mip)->std::vector<u8> {

                auto block_count = divide_up_by_multiple(mip.dimensions[0] * mip.dimensions[1], 16);

                auto need_alpha = supports_alpha(params.compression);
                auto bc_mode = params.compression == TexCompressionMode::Rgba ? BcMode::Bc7 : BcMode::Bc5;
                auto block_bts = block_bytes(bc_mode);

                std::vector<u8> compressed_bytes(block_count * block_bts, 0);

                DS_LOG_INFO("Compressing to {}", (u32)bc_mode);

                switch (bc_mode)
                {
                case asset::BcMode::Bc5:
                {

                }
                break;
                case asset::BcMode::Bc7:
                {

                }
                break;
                default:
                    break;
                }
                return mip.data;
                };

            auto swizzel = [&](RawImage& mip) {

                if (params.channel_swizzle)
                {
                    auto swizel = params.channel_swizzle.value();
                    for (auto w = 0u; w < mip.dimensions[0]; w++)
                        for (auto h = 0u; h < mip.dimensions[1]; h++)
                        {
                            if (img.format == PixelFormat::R8G8B8A8_UNorm) {
                                auto rgba = mip.get_u8(w, h);
                                std::array<u8, 4> swizel_rgba;
                                swizel_rgba[0] = rgba[swizel[0]];
                                swizel_rgba[1] = rgba[swizel[1]];
                                swizel_rgba[2] = rgba[swizel[2]];
                                swizel_rgba[3] = rgba[swizel[3]];
                                mip.put(w, h, swizel_rgba);
                            }
                            else if (img.format == PixelFormat::R32G32B32A32_Float) {
                                auto rgba = mip.get_f32(w, h);
                                std::array<f32, 4> swizel_rgba;
                                swizel_rgba[0] = rgba[swizel[0]];
                                swizel_rgba[1] = rgba[swizel[1]];
                                swizel_rgba[2] = rgba[swizel[2]];
                                swizel_rgba[3] = rgba[swizel[3]];
                                mip.put(w, h, swizel_rgba);
                            }
                            else if (img.format == PixelFormat::R16G16B16A16_Float) {
                                auto rgba = mip.get_f16(w, h);
                                std::array<f32, 4> swizel_rgba;
                                swizel_rgba[0] = rgba[swizel[0]].to_f32();
                                swizel_rgba[1] = rgba[swizel[1]].to_f32();
                                swizel_rgba[2] = rgba[swizel[2]].to_f32();
                                swizel_rgba[3] = rgba[swizel[3]].to_f32();
                                mip.put_f16(w, h, swizel_rgba);
                            }
                        }
                }
            };
            auto min_img_dim = should_compress ? 4 : 1;
            auto round_up_to_block = [=](u32 x)->u32 {
                return std::max<u32>(min_img_dim, ((x + min_img_dim - 1) / min_img_dim) * min_img_dim);
                };

            auto process_mip = [&](const RawImage& mip)->std::vector<u8> {
                RawImage mip_img;
                if (mip.width() % min_img_dim != 0 || mip.height() % min_img_dim != 0)
                {
                    auto width = round_up_to_block(mip.width());
                    auto height = round_up_to_block(mip.height());
                    mip_img = mip.resize(width, height);
                }
                else
                {
                    mip_img = mip;
                }

                swizzel(mip_img);
                if (should_compress)
                    return compress(mip_img);
                return std::move(mip_img.data);
                };

            if (params.use_mips)
            {
                auto downsample = [&](const RawImage& image) {
                    return image.resize(round_up_to_block(image.width() / 2), round_up_to_block(image.height() / 2));
                    };

                auto next = std::move(downsample(rgb_img));
                mips.push_back(process_mip(rgb_img));

                auto levels = log2_int(rgb_img.dimensions);
                for (auto l = 1u; l < levels; l++)
                {
                    mips.push_back(process_mip(next));
                    next = downsample(next);
                }
            }
            else
                mips.push_back(process_mip(rgb_img));
            upload_2_gpu(format);
            if (gpu_texture)
                set_flag(AssetFlag::UploadedGpu);
            set_flag(AssetFlag::Loaded);
        }
        void Texture::init_from_path(const std::filesystem::path& path)
        {
            file_path = path.string();
            try
            {
			    auto img = load_image(path);
			    init_from_raw_image(img, TexParams{});
            }
            catch (const std::exception& e)
            {
                DS_LOG_ERROR("Failed to load texture '{}': {}", path.string(), e.what());
                set_flag(AssetFlag::Invalid);
            }
        }
    }
}
