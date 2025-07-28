#include "restir_gi.h"
#include "radiance_cache.h"
namespace diverse
{

    RestirGiRenderer::RestirGiRenderer()
    {
        temporal_radiance_tex = PingPongTemporalResource("restirgi.radiance");
        temporal_ray_orig_tex = PingPongTemporalResource("restirgi.ray_orig");
        temporal_ray_tex = PingPongTemporalResource("restirgi.ray");
        temporal_reservoir_tex = PingPongTemporalResource("restirgi.reservoir");
        temporal_candidate_tex = PingPongTemporalResource("restirgi.candidate");
        temporal_invalidity_tex = PingPongTemporalResource("restirgi.invalidity");
        temporal2_tex = PingPongTemporalResource("restirgi.temporal2");
        temporal2_variance_tex = PingPongTemporalResource("restirgi.temporal2_var");
        temporal_hit_normal_tex = PingPongTemporalResource("restirgi.hit_normal");
        spatial_reuse_pass_count = 2;
        use_raytraced_reservoir_visibility = true;
        use_trace_eval = false;
    }

	auto RestirGiRenderer::render(rg::TemporalGraph& rg,
				ReprjoectedRestirGi&& reprojected, 
                GbufferDepth& gbuffer_depth,
				const rg::Handle<rhi::GpuTexture>& reprojection_map,
				const rg::Handle<rhi::GpuTexture>& sky_cube,
				rhi::DescriptorSet* bindless_descriptor_set,
				IracheState& surfel_state, 
                const radiance_cache::RadianceCacheState& rc_state,
				rg::Handle<rhi::GpuRayTracingAcceleration>& tlas, 
				rg::Handle<rhi::GpuTexture>& ssao_tex)->RestirGiOutput
	{
        auto& [reprojected_history_tex, temporal_output_tex] = reprojected;

         auto half_ssao_tex = rg.create<rhi::GpuTexture>(
            ssao_tex
            .get_desc()
            .half_res()
            .with_format(PixelFormat::R8_SNorm),
            "half_ssao"
            );
         rg::RenderPass::new_compute(
             rg.add_pass("extract ssao/2"),
             "/shaders/extract_half_res_ssao.hlsl"
             )
             .read(ssao_tex)
             .write(half_ssao_tex)
             .dispatch(half_ssao_tex.desc.extent);

         auto gbuffer_desc = gbuffer_depth.gbuffer.desc;
         auto half_gbuffer_desc = gbuffer_desc.half_res();
         auto [hit_normal_output_tex, hit_normal_history_tex] =
              temporal_hit_normal_tex.get_output_and_history(
                 rg,
                 temporal_tex_desc(
                     half_gbuffer_desc
                     .with_format(PixelFormat::R8G8B8A8_UNorm)
                     .extent_2d()
                     )
                 );

         auto [candidate_output_tex, candidate_history_tex] =
            temporal_candidate_tex.get_output_and_history(
             rg,
             rhi::GpuTextureDesc::new_2d(
                 PixelFormat::R16G16B16A16_Float,
                 half_gbuffer_desc.extent_2d()
                 )
             .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
             );
         auto candidate_radiance_tex = rg.create<rhi::GpuTexture>(
             half_gbuffer_desc
             .with_format(PixelFormat::R16G16B16A16_Float),
             "restirgi.candidate_radiance"
             );

         auto candidate_normal_tex = rg.create<rhi::GpuTexture>(
            half_gbuffer_desc
            .with_format(PixelFormat::R8G8B8A8_SNorm),
             "restirgi.candidate_normal"
             );

         auto candidate_hit_tex = rg.create<rhi::GpuTexture>(
             half_gbuffer_desc
             .with_format(PixelFormat::R16G16B16A16_Float),
             "restirgi.candidate_hit"
             );
         auto temporal_reservoir_packed_tex = rg.create<rhi::GpuTexture>(
             half_gbuffer_desc
             .with_format(PixelFormat::R32G32B32A32_UInt),
             "restirgi.temporal_reservoir"
             );
         auto half_depth_tex = gbuffer_depth.get_half_depth(rg);

         auto [invalidity_output_tex, invalidity_history_tex] =
             temporal_invalidity_tex.get_output_and_history(
                 rg,
                 temporal_tex_desc(half_gbuffer_desc.extent_2d())
                 .with_format(PixelFormat::R16G16_Float)
                 );
        rg::Handle<rhi::GpuTexture> radiance_tex, temporal_reservoir_tex; 
        auto half_view_normal_tex = gbuffer_depth.get_half_view_normal(rg);
        {

             auto [radiance_output_tex, radiance_history_tex] =
                    this->temporal_radiance_tex.get_output_and_history(
                     rg,
                     temporal_tex_desc(half_gbuffer_desc.extent_2d())
                     );
             auto [ray_orig_output_tex, ray_orig_history_tex] =
                    this->temporal_ray_orig_tex.get_output_and_history(
                    rg,
                    rhi::GpuTextureDesc::new_2d(
                     PixelFormat::R32G32B32A32_Float,
                     half_gbuffer_desc.extent_2d()
                     )
                 .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                 );
             auto [ray_output_tex, ray_history_tex] =
                    this->temporal_ray_tex.get_output_and_history(
                     rg,
                     temporal_tex_desc(half_gbuffer_desc.extent_2d())
                     .with_format(PixelFormat::R16G16B16A16_Float)
                     );
         
             auto rt_history_validity_pre_input_tex = rg.create<rhi::GpuTexture>(
                half_gbuffer_desc
                .with_format(PixelFormat::R8_UNorm),
                 "restirgi.rt_history_validity"
                 );
        
             auto [reservoir_output_tex, reservoir_history_tex] =
                    this->temporal_reservoir_tex.get_output_and_history(
                     rg,
                     rhi::GpuTextureDesc::new_2d(PixelFormat::R32G32_UInt, half_gbuffer_desc.extent_2d())
                     .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                     );

             rg::RenderPass::new_rt(
                 rg.add_pass("restirgi validate"),
                 rhi::ShaderSource{"/shaders/rtdgi/diffuse_validate.rgen.hlsl"},
                 {
                     rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                     rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
                 },
                 {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
                 )
                 .read(half_view_normal_tex)
                 .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                 .read(reprojected_history_tex)
                 .write(reservoir_history_tex)
                 .read(ray_history_tex)
                 .read(reprojection_map)
                 .bind(surfel_state)
                 .bind(rc_state)
                 .read(sky_cube)
                 .write(radiance_history_tex)
                 .read(ray_orig_history_tex)
                 .write(rt_history_validity_pre_input_tex)
                 .constants(gbuffer_desc.extent_inv_extent_2d())
                 .raw_descriptor_set(1, bindless_descriptor_set)
                 .trace_rays(tlas, candidate_radiance_tex.desc.extent);

             auto rt_history_validity_input_tex =
                 rg.create<rhi::GpuTexture>(half_gbuffer_desc.with_format(PixelFormat::R8_UNorm));

             rg::RenderPass::new_rt(
                 rg.add_pass("restirgi trace"),
                 rhi::ShaderSource{"/shaders/rtdgi/trace_diffuse.rgen.hlsl"},
                 {
                     rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                     rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
                 },
                 {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
                 )
                 .read(half_view_normal_tex)
                 .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                 .read(reprojected_history_tex)
                 .read(reprojection_map)
                 .bind(surfel_state)
                 .bind(rc_state)
                 .read(sky_cube)
                 .read(ray_orig_history_tex)
                 .write(candidate_radiance_tex)
                 .write(candidate_normal_tex)
                 .write(candidate_hit_tex)
                 .read(rt_history_validity_pre_input_tex)
                 .write(rt_history_validity_input_tex)
                 .constants(gbuffer_desc.extent_inv_extent_2d())
                 .raw_descriptor_set(1, bindless_descriptor_set)
                 .trace_rays(tlas, candidate_radiance_tex.desc.extent);

             rg::RenderPass::new_compute(
                 rg.add_pass("validity integrate"),
                 "/shaders/rtdgi/temporal_validity_integrate.hlsl"
                 )
                 .read(rt_history_validity_input_tex)
                 .read(invalidity_history_tex)
                 .read(reprojection_map)
                 .read(half_view_normal_tex)
                 .read(half_depth_tex)
                 .write(invalidity_output_tex)
                 .constants(std::tuple{
                     gbuffer_desc.extent_inv_extent_2d(),
                     invalidity_output_tex.desc.extent_inv_extent_2d()
                     })
                 .dispatch(invalidity_output_tex.desc.extent);

             rg::RenderPass::new_compute(
                 rg.add_pass("restir temporal"),
                 "/shaders/rtdgi/restir_temporal.hlsl"
                 )
                 .read(half_view_normal_tex)
                 .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                 .read(candidate_radiance_tex)
                 .read(candidate_normal_tex)
                 .read(candidate_hit_tex)
                 .read(radiance_history_tex)
                 .read(ray_orig_history_tex)
                 .read(ray_history_tex)
                 .read(reservoir_history_tex)
                 .read(reprojection_map)
                 .read(hit_normal_history_tex)
                 .read(candidate_history_tex)
                 .read(invalidity_output_tex)
                 .write(radiance_output_tex)
                 .write(ray_orig_output_tex)
                 .write(ray_output_tex)
                 .write(hit_normal_output_tex)
                 .write(reservoir_output_tex)
                 .write(candidate_output_tex)
                 .write(temporal_reservoir_packed_tex)
                 .constants(gbuffer_desc.extent_inv_extent_2d())
                 .raw_descriptor_set(1, bindless_descriptor_set)
                 .dispatch(radiance_output_tex.desc.extent);
               radiance_tex = std::move(radiance_output_tex); 
               temporal_reservoir_tex = std::move(reservoir_output_tex);
        }
        rg::Handle<rhi::GpuTexture> irradiance_tex;
        {
            auto reservoir_output_tex0 = rg.create<rhi::GpuTexture>(
                half_gbuffer_desc
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                .with_format(PixelFormat::R32G32_UInt),
                "restirgi.reservoir_output0"
                );
            auto reservoir_output_tex1 = rg.create<rhi::GpuTexture>(
                half_gbuffer_desc
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                .with_format(PixelFormat::R32G32_UInt),
                "restirgi.reservoir_out1"
                );
            auto bounced_radiance_output_tex0 = rg.create<rhi::GpuTexture>(
                half_gbuffer_desc
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                .with_format(PixelFormat::R11G11B10_Float),
                "restirgi.bounced_radiance_output0"
            );
            auto bounced_radiance_output_tex1 = rg.create<rhi::GpuTexture>(
                half_gbuffer_desc
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                .with_format(PixelFormat::R11G11B10_Float),
                "restirgi.bounced_radiance_output1"
                );
            auto reservoir_input_tex = &temporal_reservoir_tex;
            auto bounced_radiance_input_tex = &radiance_tex;

            for (u32 spatial_reuse_pass_idx = 0; spatial_reuse_pass_idx < spatial_reuse_pass_count; spatial_reuse_pass_idx++)
            {
                auto perform_occulsion_raymarch = spatial_reuse_pass_idx + 1  == spatial_reuse_pass_count ? 1 : 0;
                auto occlusion_raymarch_importance_only = use_raytraced_reservoir_visibility ? 1 : 0;

                rg::RenderPass::new_compute(
                    rg.add_pass("restir spatial"),
                    "/shaders/rtdgi/restir_spatial.hlsl"
                    )
                    .read(*reservoir_input_tex)
                    .read(*bounced_radiance_input_tex)
                    .read(half_view_normal_tex)
                    .read(half_depth_tex)
                    .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                    .read(half_ssao_tex)
                    .read(temporal_reservoir_packed_tex)
                    .read(reprojected_history_tex)
                    .write(reservoir_output_tex0)
                    .write(bounced_radiance_output_tex0)
                    .constants(std::tuple{
                        gbuffer_desc.extent_inv_extent_2d(),
                        reservoir_output_tex0.desc.extent_inv_extent_2d(),
                        spatial_reuse_pass_idx,
                        perform_occulsion_raymarch,
                        occlusion_raymarch_importance_only,
                        0
                        })
                    .dispatch(reservoir_output_tex0.desc.extent);
                std::swap(reservoir_output_tex0, reservoir_output_tex1);
                std::swap(bounced_radiance_output_tex0,bounced_radiance_output_tex1);

                reservoir_input_tex = &reservoir_output_tex1;
                bounced_radiance_input_tex = &bounced_radiance_output_tex1;
            }

            if (use_raytraced_reservoir_visibility)
            {
                rg::RenderPass::new_rt(
                    rg.add_pass("restir check"),
                    rhi::ShaderSource{"/shaders/rtdgi/restir_check.rgen.hlsl"},
                    {
                        rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                        rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
                    },
                    {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
                    )
                    .read(half_depth_tex)
                    .read(temporal_reservoir_packed_tex)
                    .write(*reservoir_input_tex)
                    .constants(gbuffer_desc.extent_inv_extent_2d())
                    .raw_descriptor_set(1, bindless_descriptor_set)
                    .trace_rays(tlas, candidate_radiance_tex.desc.extent);
            }

            auto irradiance_output_tex = rg.create<rhi::GpuTexture>(
                gbuffer_desc
                .with_format(PixelFormat::R16G16B16A16_Float),
                "restirgi.irradiance_output"
                );
          
            if (use_trace_eval)
            {
                auto eval_output_tex = rg.create<rhi::GpuTexture>(
                    half_gbuffer_desc
                    .with_format(PixelFormat::R16G16B16A16_Float),
                    "restirgi.eval_output"
                    );

                rg::RenderPass::new_rt(rg.add_pass("restir eval"),
                    rhi::ShaderSource{"/shaders/rtdgi/restir_eval.rgen.hlsl"},
                    {
                        rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                        rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
                    },
                    {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}} )
                    .read(radiance_tex)
                    .read(*reservoir_input_tex)
                    .read(gbuffer_depth.gbuffer)
                    .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                    .read(half_view_normal_tex)
                    .read(half_depth_tex)
                    .read(ssao_tex)
                    .read(temporal_reservoir_packed_tex)
                    .write(eval_output_tex)
                    .raw_descriptor_set(1, bindless_descriptor_set)
                    .constants(std::tuple{
                        gbuffer_desc.extent_inv_extent_2d(),
                        eval_output_tex.desc.extent_inv_extent_2d()
                     })
                     .trace_rays(tlas, eval_output_tex.desc.extent);

                rg::RenderPass::new_compute(
                    rg.add_pass("restir pre spatial filter"),
                    "/shaders/rtdgi/pre_spatial_filter.hlsl"
                    )
                    .read(eval_output_tex)
                    .read(gbuffer_depth.gbuffer)
                    .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                    .read(half_view_normal_tex)
                    .read(half_depth_tex)
                    .read(ssao_tex)
                    .write(irradiance_output_tex)
                    .raw_descriptor_set(1, bindless_descriptor_set)
                    .constants(std::tuple{
                        gbuffer_desc.extent_inv_extent_2d(),
                        irradiance_output_tex.desc.extent_inv_extent_2d()
                        })
                    .dispatch(irradiance_output_tex.desc.extent);

            }
            else
            {
                rg::RenderPass::new_compute(
                    rg.add_pass("restir resolve"),
                    "/shaders/rtdgi/restir_resolve.hlsl"
                    )
                    .read(radiance_tex)
                    .read(*reservoir_input_tex)
                    .read(gbuffer_depth.gbuffer)
                    .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                    .read(half_view_normal_tex)
                    .read(half_depth_tex)
                    .read(ssao_tex)
                    .read(candidate_radiance_tex)
                    .read(candidate_hit_tex)
                    .read(temporal_reservoir_packed_tex)
                    .read(*bounced_radiance_input_tex)
                    .write(irradiance_output_tex)
                    .raw_descriptor_set(1, bindless_descriptor_set)
                    .constants(std::tuple{
                        gbuffer_desc.extent_inv_extent_2d(),
                        irradiance_output_tex.desc.extent_inv_extent_2d()
                        })
                    .dispatch(irradiance_output_tex.desc.extent);
            }
            irradiance_tex = std::move(irradiance_output_tex);
        }
        auto filtered_tex = this->temporal(
            rg,
            irradiance_tex,
            gbuffer_depth,
            reprojection_map,
            reprojected_history_tex,
            invalidity_output_tex,
            temporal_output_tex
            );

        filtered_tex = this->spatial(
            rg,
            filtered_tex,
            gbuffer_depth,
            ssao_tex,
            bindless_descriptor_set
            );

        return RestirGiOutput{
            filtered_tex,
            RestirCandidates {
                candidate_radiance_tex,
                candidate_normal_tex,
                candidate_hit_tex
            }
         };
	}

    auto RestirGiRenderer::temporal(rg::TemporalGraph& rg,
        const rg::Handle<rhi::GpuTexture>& input_color,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& reprojection_map,
        const rg::Handle<rhi::GpuTexture>& reprojected_history_tex,
        const rg::Handle<rhi::GpuTexture>& rt_history_invalidity_tex,
        rg::Handle<rhi::GpuTexture>& temporal_output_tex) -> rg::Handle<rhi::GpuTexture>
    {
        auto [temporal_variance_output_tex, variance_history_tex] =
            temporal2_variance_tex.get_output_and_history(
                rg,
                rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16_Float, input_color.desc.extent_2d())
                .with_usage(rhi::TextureUsageFlags::SAMPLED | rhi::TextureUsageFlags::STORAGE)
                );
        
        auto desc = gbuffer_depth.gbuffer.desc;
     
        auto temporal_filtered_tex = rg.create<rhi::GpuTexture>(
            desc
            .with_format(PixelFormat::R16G16B16A16_Float),
            "restirgi.temporal_filtered"
            );

        rg::RenderPass::new_compute(
            rg.add_pass("restir temporal filter"),
            "/shaders/rtdgi/temporal_filter.hlsl"
            )
            .read(input_color)
            .read(reprojected_history_tex)
            .read(variance_history_tex)
            .read(reprojection_map)
            .read(rt_history_invalidity_tex)
            .write(temporal_filtered_tex)
            .write(temporal_output_tex)
            .write(temporal_variance_output_tex)
            .constants(std::tuple{
                temporal_output_tex.desc.extent_inv_extent_2d(),
                gbuffer_depth.gbuffer.desc.extent_inv_extent_2d()
                })
            .dispatch(temporal_output_tex.desc.extent);

        return temporal_filtered_tex;
    }

    auto RestirGiRenderer::spatial(rg::TemporalGraph& rg,
        const rg::Handle<rhi::GpuTexture>& input_color,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& ssao_tex,
        rhi::DescriptorSet*  bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>
    {
        auto spatial_filtered_tex =
            rg.create<rhi::GpuTexture>(temporal_tex_desc(input_color.desc.extent_2d()),"restirgi.spatial_filtered");

        rg::RenderPass::new_compute(
            rg.add_pass("restirgi spatial filter"),
            "/shaders/rtdgi/spatial_filter.hlsl"
            )
            .read(input_color)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(ssao_tex)
            .read(gbuffer_depth.geometric_normal)
            .write(spatial_filtered_tex)
            .constants(spatial_filtered_tex.desc.extent_inv_extent_2d())
            .raw_descriptor_set(1, bindless_descriptor_set)
            .dispatch(spatial_filtered_tex.desc.extent);

        return spatial_filtered_tex;
    }

    auto RestirGiRenderer::reproject(rg::TemporalGraph& rg,
        const rg::Handle<rhi::GpuTexture>& reprojection_map) -> ReprjoectedRestirGi
    {
        auto gbuffer_extent = reprojection_map.desc.extent_2d();

        auto [temporal_output_tex, history_tex] = temporal2_tex
            .get_output_and_history(rg, temporal_tex_desc(gbuffer_extent));

        auto reprojected_history_tex = rg.create<rhi::GpuTexture>(temporal_tex_desc(gbuffer_extent),"restirgi.reprojected");

        rg::RenderPass::new_compute(
            rg.add_pass("rtdgi reproject"),
            "/shaders/rtdgi/fullres_reproject.hlsl"
            )
            .read(history_tex)
            .read(reprojection_map)
            .write(reprojected_history_tex)
            .constants(reprojected_history_tex.desc.extent_inv_extent_2d())
            .dispatch(reprojected_history_tex.desc.extent);

        return ReprjoectedRestirGi{
            reprojected_history_tex,
            temporal_output_tex
        };
    }
}