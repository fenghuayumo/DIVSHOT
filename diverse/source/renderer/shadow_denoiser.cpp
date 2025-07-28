#include "shadow_denoiser.h"

namespace diverse
{
    auto trace_sun_shadow_mask(
        rg::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
        rhi::DescriptorSet* bindless_descriptor_set
        ) -> rg::Handle<rhi::GpuTexture> 
    {
        auto desc = gbuffer_depth.geometric_normal.desc;
        auto output_img = rg.create<rhi::GpuTexture>(desc.with_format(PixelFormat::R8_UNorm));

        rg::RenderPass::new_rt(
            rg.add_pass("trace shadow mask"),
            rhi::ShaderSource{"/shaders/rt/trace_sun_shadow_mask.rgen.hlsl"},
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {}
            )
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(gbuffer_depth.geometric_normal)
            .write(output_img)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, output_img.desc.extent);

        return output_img;
    }

    auto ShadowDenoiser::filter_spatial(
        rg::TemporalGraph& rg,
        u32 step_size,
        const rg::Handle<rhi::GpuTexture>& input_image,
        rg::Handle<rhi::GpuTexture>& output_image,
        const rg::Handle<rhi::GpuTexture>& metadata_image,
        const GbufferDepth& gbuffer_depth,
        const std::array<u32, 2> bitpacked_shadow_mask_extent
    )
    {
        rg::RenderPass::new_compute(
            rg.add_pass("shadow spatial"),
            "/shaders/shadow_denoise/spatial_filter.hlsl"
            )
            .read(input_image)
            .read(metadata_image)
            .read(gbuffer_depth.geometric_normal)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .write(output_image)
            .constants(std::tuple{
                output_image.desc.extent_inv_extent_2d(),
                bitpacked_shadow_mask_extent,
                step_size,
            })
            .dispatch(output_image.desc.extent);
    }

    auto ShadowDenoiser::render(rg::TemporalGraph& rg,
			GbufferDepth& gbuffer_depth,
			const rg::Handle<rhi::GpuTexture>& shadow_mask,
			const rg::Handle<rhi::GpuTexture>& reprojection_map) -> rg::Handle<rhi::GpuTexture>
    {
        auto gbuffer_desc = gbuffer_depth.gbuffer.desc;
        auto bitpacked_shadow_mask_extent = gbuffer_depth.gbuffer.get_desc().div_up_extent({8, 4, 1}).extent_2d();
        auto bitpacked_shadows_image = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
            PixelFormat::R32_UInt,
            bitpacked_shadow_mask_extent),
            "bitpacked_shadows_image"
            );

        rg::RenderPass::new_compute(
            rg.add_pass("shadow bitpack"),
            "/shaders/shadow_denoise/bitpack_shadow_mask.hlsl"
            )
            .read(shadow_mask)
            .write(bitpacked_shadows_image)
            .constants(std::tuple{
                gbuffer_desc.extent_inv_extent_2d(),
                bitpacked_shadow_mask_extent
                })
            .dispatch({
                bitpacked_shadow_mask_extent[0] * 2,
                bitpacked_shadow_mask_extent[1],
                1
            });

        auto [moments_image, prev_moments_image] = moments.get_output_and_history(
            rg,
            gbuffer_desc
            .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
            .with_format(PixelFormat::R16G16B16A16_Float));

        auto spatial_image_desc = rhi::GpuTextureDesc::new_2d(
            PixelFormat::R16G16_Float, gbuffer_depth.depth.desc.extent_2d());
        auto [accum_image, prev_accum_image] = accum.get_output_and_history(
            rg, spatial_image_desc.with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
        );

        auto spatial_input_image = rg.create<rhi::GpuTexture>(spatial_image_desc,"shadow_denoise.spatial_input");
        auto metadata_image = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(
            PixelFormat::R32_UInt,
            bitpacked_shadow_mask_extent
        ));

        rg::RenderPass::new_compute(
            rg.add_pass("shadow temporal"),
            "/shaders/shadow_denoise/megakernel.hlsl"
            )
            .read(shadow_mask)
            .read(bitpacked_shadows_image)
            .read(prev_moments_image)
            .read(prev_accum_image)
            .read(reprojection_map)
            .write(moments_image)
            .write(spatial_input_image)
            .write(metadata_image)
            .constants(std::tuple{
                gbuffer_desc.extent_inv_extent_2d(),
                bitpacked_shadow_mask_extent,
            })
            .dispatch(gbuffer_desc.extent);


        auto temp = rg.create<rhi::GpuTexture>(spatial_image_desc);

        filter_spatial(
            rg,
            1,
            spatial_input_image,
            accum_image,
            metadata_image,
            gbuffer_depth,
            bitpacked_shadow_mask_extent
        );
        filter_spatial(
            rg,
            2,
            accum_image,
            temp,
            metadata_image,
            gbuffer_depth,
            bitpacked_shadow_mask_extent
        );
        filter_spatial(
            rg,
            4,
            temp,
            spatial_input_image,
            metadata_image,
            gbuffer_depth,
            bitpacked_shadow_mask_extent
        );

        return spatial_input_image;
    }
}
