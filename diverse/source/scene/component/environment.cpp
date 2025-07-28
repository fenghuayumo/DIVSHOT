#include "environment.h"
#include "assets/texture.h"
#include "assets/asset_manager.h"
#include "backend/drs_rhi/gpu_texture.h"
#include "backend/drs_rhi/gpu_device.h"
namespace diverse
{
	Environment::Environment()
	{
	}

	void Environment::load_hdr(const std::string& path)
	{
		file_path = path;
		load();
	}

	void Environment::load()
	{
		if( std::filesystem::exists(file_path ))
		{
			auto raw_img = asset::load_float_image(file_path).convert(PixelFormat::R16G16B16A16_Float);
			auto img_desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, raw_img.dimensions)
				.with_usage(rhi::TextureUsageFlags::SAMPLED);
			const u32 PIXEL_BYTES = 4 * 2;
			rhi::ImageSubData	sub_data = {
				(u8*)raw_img.data.data(),
				(u32)raw_img.data.size(),
				raw_img.dimensions[0] * PIXEL_BYTES,
				raw_img.dimensions[0] * raw_img.dimensions[1] * PIXEL_BYTES
			};

			hdr_img = g_device->create_texture(img_desc, { sub_data }, "ibl");
		}
	}	

	auto Environment::create_gray_tex() -> std::shared_ptr<rhi::GpuTexture>
	{
		auto raw_img = asset::RawImage{ PixelFormat::R16G16B16A16_Float, {1, 1}, std::vector<u8>(8,0)};
		raw_img.put_f16(0, 0, std::array<f32,4>{ 0.5f, 0.5f, 0.5f, 1.0f });
		auto img_desc = rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, raw_img.dimensions)
			.with_usage(rhi::TextureUsageFlags::SAMPLED);
		const u32 PIXEL_BYTES = 4 * 2;
		rhi::ImageSubData	sub_data = {
			(u8*)raw_img.data.data(),
			(u32)raw_img.data.size(),
			raw_img.dimensions[0] * PIXEL_BYTES,
			raw_img.dimensions[0] * raw_img.dimensions[1] * PIXEL_BYTES
		};

		return g_device->create_texture(img_desc, { sub_data }, "gray");
	}

	auto Environment::get_render_light_data(const maths::Transform& transform, struct LightShaderData* light_data) -> void
	{
		light_data->colorTypeAndFlags = (uint32_t)LightType::kEnvironment << kPolymorphicLightTypeShift;
		//packLightColor(, *light_data);
		//light_data->direction1 = (uint32_t)env.textureIndex;
		//light_data->direction2 = env.textureSize.x | (env.textureSize.y << 16);
		//light_data->scalars = fp32ToFp16(env.rotation);
		light_data->scalars |= (1 << 16);
	}
}
