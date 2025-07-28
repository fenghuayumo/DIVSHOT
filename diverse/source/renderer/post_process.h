#pragma once

#include "renderer/drs_rg/temporal.h"
#include "renderer/drs_rg/simple_pass.h"
#include <optional>
namespace diverse
{
    struct HistogramClipping
    {
        f32 low = 0.0f;
        f32 high = 0.1f;
    };

	struct ExposureState
	{
		/// A value to multiply all lighting by in order to apply exposure compensation
		/// early in the pipeline, such that lighting values fit in small texture formats.
		f32 pre_mult = 1.0f;

		/// The remaining multiplier to apply in post.
		f32 post_mult = 1.0f;

		// The pre-multiplier in the previous frame.
		f32 pre_mult_prev = 1.0f;

		// `pre_mult / pre_mult_prev`
		f32 pre_mult_delta = 1.0f;
	};

	struct DynamicExposureState
	{
	public:
		bool enabled = true;
		f32 speed_log2 = 0.0f;
		HistogramClipping histogram_clipping;
	private:
		f32 ev_fast = 0.0f;
		f32 ev_slow = 0.0f;
	public:
		auto ev_smoothed() -> f32;
		auto update(f32 ev, f32 dt) -> void;
	};
    struct PostProcessRenderer
    {
        f32 image_log2_lum = 0.0f;
        f32 contrast = 1.0f;
        std::shared_ptr<rhi::GpuBuffer> histogram_buffer;
        rhi::GpuDevice* device;
        PostProcessRenderer(rhi::GpuDevice* device);

        auto render(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& input,
            rhi::DescriptorSet* bindless_descriptor_set)->rg::Handle<rhi::GpuTexture>;

		auto update_pre_exposure()->void;

		ExposureState			expos_state;
    protected:
        auto calculate_luminance_histogram(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& blur_pyramid)
            -> rg::Handle<rhi::GpuBuffer>;

        auto read_back_histogram(const HistogramClipping& exposure_histogram_clipping) -> void;
		DynamicExposureState	dynamic_exposure;
    };
}