#include "post_process.h"
#include "drs_rg/image_op.h"
#include <algorithm>
#include <numeric>
#include <format>
#include <glm/vec2.hpp>
#include "render_settings.h"
#include <iostream>
namespace diverse
{
	const u32 LUMINANCE_HISTOGRAM_BIN_COUNT  = 256;
	const f32 LUMINANCE_HISTOGRAM_MIN_LOG2 = -16.0;
	const f32 LUMINANCE_HISTOGRAM_MAX_LOG2 = 16.0;
	const f32 DYNAMIC_EXPOSURE_BIAS = -2.0f;

	auto DynamicExposureState::ev_smoothed() -> f32
	{
		if (enabled) {
			return (ev_slow + ev_fast) * 0.5 + DYNAMIC_EXPOSURE_BIAS;
		}
		return 0.0f;
	}

	auto DynamicExposureState::update(f32 ev, f32 dt) -> void
	{
		if (!enabled) {
			return;
		}
		ev = std::clamp(ev, -16.0f, 16.0f);
		dt = dt * std::exp2(speed_log2);
		auto t_fast = 1.0 - std::exp(-1.0 * dt);
		ev_fast = (ev - this->ev_fast) * t_fast + this->ev_fast;
		auto t_slow = 1.0 - std::exp(-0.25 * dt);
		ev_slow = (ev - this->ev_slow) * t_slow + this->ev_slow;
	}

	auto blur_pyramid(rg::RenderGraph& rg, const rg::Handle<rhi::GpuTexture>& input)-> rg::Handle<rhi::GpuTexture>
	{
		auto skip_n_bottom_mips = 1;
		auto desc = input.desc;
		auto pyramid_desc = desc
			.half_res()
			.with_format(PixelFormat::R11G11B10_Float) // R16G16B16A16_SFLOAT
			.all_mip_levels();

		pyramid_desc.mip_levels = std::max<u32>(1, pyramid_desc.mip_levels - skip_n_bottom_mips);

		auto output = rg.create<rhi::GpuTexture>(pyramid_desc, "pyramid");

		rg::RenderPass::new_compute(rg.add_pass("_blur0"), "/shaders/blur.hlsl")
				.read(input)
				.write_view(output, rhi::GpuTextureViewDesc()
						.with_base_mip_level(0)
						.with_level_count(1))
				.dispatch(output.desc.extent);
	
		for (i32 target_mip = 1; target_mip < output.desc.mip_levels; target_mip++)
		{
			u32 downsample_amount = 1 << target_mip;
			rg::RenderPass::new_compute(rg.add_pass(std::format("_blur{}", target_mip)), "/shaders/blur.hlsl")
						.read_view(output,rhi::GpuTextureViewDesc()
							.with_base_mip_level(target_mip-1)
							.with_level_count(1))
						.write_view(output, rhi::GpuTextureViewDesc()
							.with_base_mip_level(target_mip)
							.with_level_count(1))
						.dispatch(output.get_desc()
							.div_extent({downsample_amount, downsample_amount, 1})
							.extent);
		}
		return output;
	}

	auto rev_blur_pyramid(rg::RenderGraph& rg,const rg::Handle<rhi::GpuTexture>& in_pyramid) -> rg::Handle<rhi::GpuTexture>
	{
		auto output = rg.create<rhi::GpuTexture>(in_pyramid.desc,"rev_blur_pyramid");
		rg::clear_color(rg,output,{0.0f,0.0f,0.0f,1.0f});
		for (i32 target_mip = (output.desc.mip_levels - 2); target_mip >= 0; target_mip--)
		{
			u32 downsample_amount = 1 << target_mip;
			auto output_extent = output.get_desc()
				.div_extent({ downsample_amount, downsample_amount, 1 })
				.extent;

			auto src_mip = target_mip + 1;
			auto self_weight = src_mip == (output.desc.mip_levels-1) ? 0.0f : 0.5f;

			rg::RenderPass::new_compute(rg.add_pass(std::format("_rev_blur{}", target_mip)), "/shaders/rev_blur.hlsl")
							.read_view(in_pyramid, rhi::GpuTextureViewDesc()
								.with_base_mip_level(target_mip)
								.with_level_count(1))
							.read_view(output, rhi::GpuTextureViewDesc()
								.with_base_mip_level(src_mip)
								.with_level_count(1))
							.write_view(output, rhi::GpuTextureViewDesc()
								.with_base_mip_level(target_mip)
								.with_level_count(1))
							.constants(std::tuple{output_extent[0], output_extent[1], self_weight,0.0f})
							.dispatch(output_extent);					
		}
		return output;
	}
	
	PostProcessRenderer::PostProcessRenderer(rhi::GpuDevice* dev)
		: device(dev)
	{
		auto buffer_desc = rhi::GpuBufferDesc::new_gpu_to_cpu(sizeof(u32) * LUMINANCE_HISTOGRAM_BIN_COUNT,
			rhi::BufferUsageFlags::STORAGE_BUFFER);
		auto histogram = std::array<u32, LUMINANCE_HISTOGRAM_BIN_COUNT>{0};
		histogram_buffer = device->create_buffer(buffer_desc, "luminance histogram", (u8*)histogram.data());
		image_log2_lum = 0.0f;
		contrast = 1.0f;
	}

	auto PostProcessRenderer::calculate_luminance_histogram(rg::RenderGraph& rg, const rg::Handle<rhi::GpuTexture>& blur_pyramid) -> rg::Handle<rhi::GpuBuffer>
	{
		auto buffer_desc = rhi::GpuBufferDesc::new_gpu_only(sizeof(u32) * LUMINANCE_HISTOGRAM_BIN_COUNT, 
							rhi::BufferUsageFlags::STORAGE_BUFFER);

		auto tmp_histogram = rg.create<rhi::GpuBuffer>(buffer_desc);

		auto input_mip_level = std::max<i32>(0, blur_pyramid.desc.mip_levels - 7);
		auto tex_desc = blur_pyramid.desc;
		auto mip_extent = tex_desc.div_up_extent({ (u32)1 << input_mip_level , (u32)1 << input_mip_level , 1}).extent;

		rg::RenderPass::new_compute(
			rg.add_pass("_clear histogram"), 
				"/shaders/post/luminance_histogram_clear.hlsl")
			.write(tmp_histogram)
			.dispatch({ LUMINANCE_HISTOGRAM_BIN_COUNT ,1,1});

		rg::RenderPass::new_compute(
			rg.add_pass("calculate histogram"), 
				"/shaders/post/luminance_histogram_calculate.hlsl")
			.read_view(blur_pyramid, rhi::GpuTextureViewDesc()
				.with_base_mip_level(input_mip_level)
				.with_level_count(1))
			.write(tmp_histogram)
			.constants(glm::uvec2{mip_extent[0], mip_extent[1]})
			.dispatch(mip_extent);

		auto dst_histogram = rg.import_res(histogram_buffer, rhi::AccessType::Nothing);
		rg::RenderPass::new_compute(
			rg.add_pass("_copy histogram"), 
				"/shaders/post/luminance_histogram_copy.hlsl")
			.read(tmp_histogram)
			.write(dst_histogram)
			.dispatch({ LUMINANCE_HISTOGRAM_BIN_COUNT ,1,1 });

		return tmp_histogram;
	}

	auto PostProcessRenderer::read_back_histogram(const HistogramClipping& exposure_histogram_clipping) -> void
	{
		auto histogram = std::array<u32, LUMINANCE_HISTOGRAM_BIN_COUNT>{0};
		auto src = histogram_buffer->map(device);
		memcpy(histogram.data(), src, sizeof(u32) * LUMINANCE_HISTOGRAM_BIN_COUNT);
		histogram_buffer->unmap(device);

		auto outlier_frac_lo = std::min(exposure_histogram_clipping.low, 1.0f);
		auto outlier_frac_hi = std::min(exposure_histogram_clipping.high, 1.0f - outlier_frac_lo);
		auto total_entry_count = std::accumulate(histogram.begin(), histogram.end(), 0);
		auto reject_lo_entry_count = i32(total_entry_count  * outlier_frac_lo) ;
		auto entry_count_to_use =
			i32(total_entry_count * (1.0 - outlier_frac_lo - outlier_frac_hi));
		f64 sum = 0.0;
		u32 used_count = 0;
		auto left_to_reject = reject_lo_entry_count;
		auto left_to_use = entry_count_to_use;

		for (auto bin_idx = 0; bin_idx < histogram.size(); bin_idx++)
		{
			f64 t = (static_cast<f64>(bin_idx)  + 0.5) / LUMINANCE_HISTOGRAM_BIN_COUNT;
			i32 count = histogram[bin_idx];
			auto count_to_use = std::min(std::max<i32>(count - left_to_reject,0),left_to_use);
			left_to_reject = std::max<i32>(left_to_reject - count,0);
			left_to_use = std::max<i32>(left_to_use - count_to_use, 0);
			sum += t * count_to_use;
			used_count += count_to_use;
		}
		assert(entry_count_to_use == used_count);

		auto mean =  sum / static_cast<f64>(std::max<u32>(used_count,1));
		image_log2_lum = (LUMINANCE_HISTOGRAM_MIN_LOG2
			+ mean * (LUMINANCE_HISTOGRAM_MAX_LOG2 - LUMINANCE_HISTOGRAM_MIN_LOG2));
	}
	
	auto PostProcessRenderer::render(rg::RenderGraph& rg, const rg::Handle<rhi::GpuTexture>& input, rhi::DescriptorSet* bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>
	{
		contrast = g_render_settings.contrast;
		dynamic_exposure.histogram_clipping = {g_render_settings.dynamic_adaptation_low_clip, g_render_settings.dynamic_adaptation_high_clip};
		read_back_histogram(dynamic_exposure.histogram_clipping);
		auto blur_pyr = blur_pyramid(rg, input);
		auto histogram = calculate_luminance_histogram(rg, blur_pyr);
		auto rev_blur_pyr = rev_blur_pyramid(rg, blur_pyr);
		auto desc = input.desc;
		auto output = rg.create<rhi::GpuTexture>(desc.with_format(PixelFormat::R11G11B10_Float), "post_output");

		rg::RenderPass::new_compute(
			rg.add_pass("post combine"),
				"/shaders/post_combine.hlsl")
			.read(input)
			.read(blur_pyr)
			.read(rev_blur_pyr)
			.read(histogram)
			.write(output)
			.raw_descriptor_set(1, bindless_descriptor_set)
			.constants(std::tuple{
				output.desc.extent_inv_extent_2d(),
				expos_state.post_mult,
				contrast
				})
			.dispatch(output.desc.extent);
		return output;
	}
	auto PostProcessRenderer::update_pre_exposure() -> void
	{
		auto dt = 1.0 / 60.0f; //TODO
		dynamic_exposure.update(-image_log2_lum, dt);
		auto ev_mult = std::exp2(g_render_settings.ev_shift + dynamic_exposure.ev_smoothed());
		expos_state.pre_mult_prev = expos_state.pre_mult;
		expos_state.pre_mult = expos_state.pre_mult * 0.9 + 0.1 * ev_mult;
		expos_state.post_mult = ev_mult / expos_state.pre_mult;
		expos_state.pre_mult_delta = expos_state.pre_mult / expos_state.pre_mult_prev;
	}
}