#include "lighting.h"
#include "defered_renderer.h"
#include "scene/scene.h"
#include "scene/component/environment.h"
#include "scene/component/light/directional_light.h"
#include "scene/component/light/point_light.h"
#include "scene/component/light/spot_light.h"
#include "scene/component/light/rect_light.h"
#include "scene/component/light/cylinder_light.h"
#include "scene/component/light/disk_light.h"
#include "sky_pass.h"
#include "gbuffer_depth.h"
#include "irache.h"
#include "radiance_cache.h"
#include "scene/entity_manager.h"
#include "rtxdi/importance_sampling_context.h"
#include "rtxdi/ris_buffer_segment_allocator.h"
#define RTXDI_PRESAMPLING_GROUP_SIZE 256
#define RTXDI_GRID_BUILD_GROUP_SIZE 256
#define RTXDI_SCREEN_SPACE_GROUP_SIZE 8
#define RTXDI_GRAD_FACTOR 3
#define RTXDI_GRAD_STORAGE_SCALE 256.0f
#define RTXDI_GRAD_MAX_VALUE 65504.0f

#define DENOISER_MODE_OFF 0
#define DENOISER_MODE_REBLUR 1
#define DENOISER_MODE_RELAX 2

namespace diverse
{
    extern std::array<glm::ivec4, 16 * 4 * 8>    SPATIAL_RESOLVE_OFFSETS;

    using namespace rg;
    static const int maxEmissiveMeshes = 1024;
    static const int maxPrimitiveLights = 1024;

    struct PrepareLightsConstants
    {
        uint numTasks;
        uint currentFrameLightOffset;
        uint previousFrameLightOffset;
        uint pad;
    };
    struct ResamplingConstants
    {
        RTXDI_RuntimeParameters runtimeParams;
        // Common buffer params
        RTXDI_LightBufferParameters lightBufferParams;
        RTXDI_RISBufferSegmentParameters localLightsRISBufferSegmentParams;
        RTXDI_RISBufferSegmentParameters environmentLightRISBufferSegmentParams;

        ReSTIRDI_Parameters restirDI;
        ReGIR_Parameters regir;

        glm::uvec2 environmentPdfTextureSize;
        glm::uvec2 localLightPdfTextureSize;

        uint enableBrdfAdditiveBlend;
        uint enablePreviousTLAS;
        uint denoiserMode = 0;
        uint discountNaiveSamples;
    };
    LightingPass::LightingPass(DeferedRenderer* render)
    : renderer(render)
    {
        rhi_task_buffer = g_device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(sizeof(PrepareLightsTask) * (maxEmissiveMeshes + maxPrimitiveLights),
                rhi::BufferUsageFlags::TRANSFER_DST |
                rhi::BufferUsageFlags::STORAGE_BUFFER |
                rhi::BufferUsageFlags::TRANSFER_SRC),
            "rhi_task_buffer",nullptr
        );
        rtxdi::ImportanceSamplingContext_StaticParameters isStaticParams;
        isStaticParams.CheckerboardSamplingMode = rtxdi::CheckerboardMode::Off;
        //isStaticParams.regirStaticParams = m_ui.regirStaticParams;

        rtxdi_sampling_context = std::make_unique<rtxdi::ImportanceSamplingContext>(isStaticParams);
        auto di_context = rtxdi_sampling_context->GetReSTIRDIContext();
        auto neighborOffsetCount = rtxdi_sampling_context->GetNeighborOffsetCount();
        std::vector<uint8_t> offsets;
        offsets.resize(neighborOffsetCount * 2);
        rtxdi::FillNeighborOffsetBuffer(offsets.data(), neighborOffsetCount);
        neighbor_offsets_buf = g_device->create_buffer(
            rhi::GpuBufferDesc::new_cpu_to_gpu(offsets.size(),
            rhi::BufferUsageFlags::UNIFORM_TEXEL_BUFFER |
            rhi::BufferUsageFlags::TRANSFER_DST).with_format(PixelFormat::R32G32_Float),
            "neighbor_offsets_buf", 
            offsets.data()
        );
        
        di_context.SetResamplingMode(rtxdi::ReSTIRDI_ResamplingMode::TemporalAndSpatial);
        ReSTIRDI_InitialSamplingParameters  initialSamplingParams;
        initialSamplingParams.localLightSamplingMode = ReSTIRDI_LocalLightSamplingMode::Uniform;
        initialSamplingParams.numPrimaryLocalLightSamples = 16;
        initialSamplingParams.numPrimaryBrdfSamples = 1;
        initialSamplingParams.numPrimaryInfiniteLightSamples = 16;
        di_context.SetInitialSamplingParameters(initialSamplingParams);
        ReSTIRDI_TemporalResamplingParameters   temporalResamplingParams;
        temporalResamplingParams.enableBoilingFilter = false;
        temporalResamplingParams.boilingFilterStrength = 0.0f;
        di_context.SetTemporalResamplingParameters(temporalResamplingParams);

        di_context.SetSpatialResamplingParameters(rtxdi::GetDefaultReSTIRDISpatialResamplingParams());
        di_context.SetShadingParameters(rtxdi::GetDefaultReSTIRDIShadingParams());
    }
    LightingPass::~LightingPass()
    {
        rtxdi_sampling_context.reset();
    }

    auto LightingPass::lighting_gbuffer(rg::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& shadow_mask,
        const rg::Handle<rhi::GpuTexture>& rtr,
        const rg::Handle<rhi::GpuTexture>& rtgi,
        IracheState& surfel_state,
        const radiance_cache::RadianceCacheState& rc_state,
        rg::Handle<rhi::GpuTexture>& temporal_output,
        rg::Handle<rhi::GpuTexture>& output,
        const rg::Handle<rhi::GpuTexture>& reproject_tex,
        const rg::Handle<rhi::GpuTexture>& sky_cube,
        const rg::Handle<rhi::GpuTexture>& convolved_sky_cube,
        rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
        rhi::DescriptorSet*  bindless_descriptor_set,
        u32 debug_shading_mode) -> void
    {
        // sample_light_rendering(
        //     rg,
        //     gbuffer_depth,
        //     sky_cube,
        //     tlas,
        //     output,
        //     reproject_tex,
        //     bindless_descriptor_set);

        rg::RenderPass::new_compute(rg.add_pass("light gbuffer"), "/shaders/light_gbuffer.hlsl")
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(shadow_mask)
            .read(rtr)
            .read(rtgi)
            .bind(surfel_state)
            .bind(rc_state)
            .write(temporal_output)
            .write(output)
            .read(sky_cube)
            .read(convolved_sky_cube)
            .constants(std::tuple{
                gbuffer_depth.gbuffer.desc.extent_inv_extent_2d(),
                debug_shading_mode,
                0
                })
            .raw_descriptor_set(1, bindless_descriptor_set)
            .dispatch(gbuffer_depth.gbuffer.desc.extent);
    }

    auto build_mip_map(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuTexture>& dst_tex,
        int start_mip_level) ->void
    {
        const auto& dest_desc = dst_tex.desc;
        constexpr uint32_t mip_levels_per_pass = 5;
        uint32_t width = dest_desc.extent[0];
        uint32_t height = dest_desc.extent[1];
        for (uint32_t source_mip_level = start_mip_level; source_mip_level < dest_desc.mip_levels; source_mip_level += mip_levels_per_pass)
        {
            rg::RenderPass::new_compute(rg.add_pass("build mipmap"), "/shaders/sky/build_mipmap.hlsl")
                .write_view(dst_tex,rhi::GpuTextureViewDesc()
            	    .with_base_mip_level(source_mip_level)
                    .with_level_count(mip_levels_per_pass))
                .write_view(dst_tex, rhi::GpuTextureViewDesc()
                    .with_base_mip_level(source_mip_level + 1)
                    .with_level_count(mip_levels_per_pass))
                .write_view(dst_tex, rhi::GpuTextureViewDesc()
                    .with_base_mip_level(source_mip_level + 2)
                    .with_level_count(mip_levels_per_pass))
                .write_view(dst_tex, rhi::GpuTextureViewDesc()
                    .with_base_mip_level(source_mip_level + 3)
                    .with_level_count(mip_levels_per_pass))
                .write_view(dst_tex, rhi::GpuTextureViewDesc()
                    .with_base_mip_level(source_mip_level + 4)
                    .with_level_count(mip_levels_per_pass))
                .read_view(dst_tex, rhi::GpuTextureViewDesc()
                    .with_base_mip_level(source_mip_level - 1)
                    .with_level_count(1))
                .constants(glm::uvec4(source_mip_level,width,0,0))
                .dispatch({width, height, 1});
        }
    }

    auto LightingPass::prepare_lights(
            rg::RenderGraph& rg,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            rg::Handle<rhi::GpuTexture>& output,
            rhi::DescriptorSet* bindless_descriptor_set)->RtxDiResouceHandle
    {
        auto [sky_env_map,sky_pdf] = sky::build_sky_env_map(rg, sky_cube, 2048);
        build_mip_map(rg,sky_pdf,1);
        auto render_width = output.desc.extent[0];
        auto render_height = output.desc.extent[1];
        ///* The light indexing members of frameParameters are written by PrepareLightsPass below
        // When indirect lighting is enabled, we don't want ReSTIR to be the NRD front-end,
        // it should just write out the raw color data.*/
        ReSTIRDI_ShadingParameters restirDIShadingParams = rtxdi_sampling_context->GetReSTIRDIContext().GetShadingParameters();
        restirDIShadingParams.enableDenoiserInputPacking = false;

        rtxdi::ReSTIRDIContext& restirDIContext = rtxdi_sampling_context->GetReSTIRDIContext();
        restirDIContext.SetFrameIndex(renderer->frame_idx);
        restirDIContext.SetReservoirBufferParameters(rtxdi::CalculateReservoirBufferParameters(render_width, render_height, rtxdi::CheckerboardMode::Off));
        RTXDI_LightBufferParameters outLightBufferParams = {};
        outLightBufferParams.localLightBufferRegion.numLights += num_finite_primLights;
        outLightBufferParams.infiniteLightBufferRegion.firstLightIndex = outLightBufferParams.localLightBufferRegion.numLights;
        outLightBufferParams.infiniteLightBufferRegion.numLights = num_infinite_prim_lights;
        outLightBufferParams.environmentLightParams.lightIndex = outLightBufferParams.infiniteLightBufferRegion.firstLightIndex + outLightBufferParams.infiniteLightBufferRegion.numLights;
        outLightBufferParams.environmentLightParams.lightPresent = num_importance_sampled_environment_lights;

        u32 num_local_lights = std::max<u32>(num_infinite_prim_lights + num_finite_primLights + num_importance_sampled_environment_lights + num_emissive_triangles,1);
        auto lightBufferElements = num_local_lights * 2;
        auto light_index_map_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            (sizeof(u32) * lightBufferElements),
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::UNIFORM_TEXEL_BUFFER |
            rhi::BufferUsageFlags::STORAGE_TEXEL_BUFFER).with_format(PixelFormat::R32_UInt), "light_index_map");
        auto light_data_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            (sizeof(LightShaderData) * lightBufferElements),
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::STORAGE_BUFFER), "light_data_buf");
        auto light_reservoir_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            (sizeof(RTXDI_PackedDIReservoir) *  restirDIContext.GetReservoirBufferParameters().reservoirArrayPitch * rtxdi::c_NumReSTIRDIReservoirBuffers),
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::STORAGE_BUFFER), "light_reservoir_buf");
        auto ris_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            sizeof(uint32_t) * 2 * std::max(rtxdi_sampling_context->GetRISBufferSegmentAllocator().getTotalSizeInElements(), 1u),
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::STORAGE_TEXEL_BUFFER).with_format(PixelFormat::R32G32_UInt), "ris_buffer");
        auto ris_light_data_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            sizeof(uint32_t) * 8 * std::max(rtxdi_sampling_context->GetRISBufferSegmentAllocator().getTotalSizeInElements(), 1u),
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::STORAGE_TEXEL_BUFFER).with_format(PixelFormat::R32G32B32A32_UInt), "ris_light_data_buffer");
        u32 tex_width,tex_height,mip_levels;
        rtxdi::ComputePdfTextureSize(num_local_lights, tex_width, tex_height, mip_levels);
        auto local_light_pdf_texture = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32_Float,{ tex_width ,tex_height }).with_mip_levels(mip_levels));
        auto neighbor_offsets = rg.import_res<rhi::GpuBuffer>(neighbor_offsets_buf,rhi::AccessType::Nothing);

        PrepareLightsConstants p_constants;
        p_constants.numTasks = num_finite_primLights + num_infinite_prim_lights + num_importance_sampled_environment_lights + num_mesh_lights;
        p_constants.currentFrameLightOffset = lightBufferElements * odd_frame;
        p_constants.previousFrameLightOffset = lightBufferElements * !odd_frame;
        if(light_buffer_offset > 0)
        { 
            auto task_buffer = rg.import_res<rhi::GpuBuffer>(rhi_task_buffer, rhi::AccessType::Nothing);
            rg::RenderPass::new_compute(rg.add_pass("prepare lights"),"/shaders/rtxdi/prepare_lights.hlsl",{})
                .read(task_buffer)
                .write(light_data_buffer)
                .write(light_index_map_buffer)
                .write(local_light_pdf_texture)
                .constants(p_constants)
                .raw_descriptor_set(1, bindless_descriptor_set)
                .dispatch({ light_buffer_offset ,1,1});
        }
        outLightBufferParams.localLightBufferRegion.firstLightIndex += p_constants.currentFrameLightOffset;
        outLightBufferParams.infiniteLightBufferRegion.firstLightIndex += p_constants.currentFrameLightOffset;
        outLightBufferParams.environmentLightParams.lightIndex += p_constants.currentFrameLightOffset;
        rtxdi_sampling_context->SetLightBufferParams(outLightBufferParams);
        auto initialSamplingParams = restirDIContext.GetInitialSamplingParameters();
        initialSamplingParams.environmentMapImportanceSampling = outLightBufferParams.environmentLightParams.lightPresent;
        restirDIContext.SetInitialSamplingParameters(initialSamplingParams);
        odd_frame = !odd_frame;
        if (rtxdi_sampling_context->IsReGIREnabled() && light_buffer_offset > 0)
        {
            build_mip_map(rg, local_light_pdf_texture, 1);
        }
        auto rtxdi_resource = RtxDiResouceHandle{
            sky_env_map,
            sky_pdf,
            local_light_pdf_texture,
            light_data_buffer,
            neighbor_offsets,
            light_index_map_buffer,
            light_reservoir_buffer,
            ris_buffer,
            ris_light_data_buffer
        };
        return rtxdi_resource;
    }
    auto restir_lighting(
        rg::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& sky_cube,
        rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
        rg::Handle<rhi::GpuTexture>& output,
        const rg::Handle<rhi::GpuTexture>& reproject_tex,
        rhi::DescriptorSet* bindless_descriptor_set,
        RtxDiResouceHandle& rtxdi_resource,
        const ResamplingConstants& sampling_constants) -> void
    {
        //generate initial samples
        auto diffuse = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, output.desc.extent_2d()));
        auto specular = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float, output.desc.extent_2d()));
        rg::RenderPass::new_rt(
            rg.add_pass("initial light samples"),
            rhi::ShaderSource{ "/shaders/rtxdi/generate_initial_sampling_kernel.hlsl" },
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {}
            )
            .constants(sampling_constants)
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(rtxdi_resource.env_map_tex)
            .read(rtxdi_resource.env_map_pdf_tex)
            .read(rtxdi_resource.local_light_pdf_tex)
            .read(rtxdi_resource.light_data_buf)
            .read(rtxdi_resource.neighbor_offsets_buf)
            .read(rtxdi_resource.light_index_mapping_buf)
            .write(rtxdi_resource.light_reservoirs_buf)
            .write(rtxdi_resource.ris_buf)
            .write(rtxdi_resource.ris_light_data_buf)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, output.desc.extent);
        //temporal resampling
        rg::RenderPass::new_rt(
            rg.add_pass("rtxdi temporal resampling"),
            rhi::ShaderSource{ "/shaders/rtxdi/temporal_resampling_kernel.hlsl" },
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {}
            )
            .constants(sampling_constants)
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(rtxdi_resource.env_map_tex)
            .read(rtxdi_resource.env_map_pdf_tex)
            .read(rtxdi_resource.local_light_pdf_tex)
            .read(rtxdi_resource.light_data_buf)
            .read(rtxdi_resource.neighbor_offsets_buf)
            .read(rtxdi_resource.light_index_mapping_buf)
            .write(rtxdi_resource.light_reservoirs_buf)
            .write(rtxdi_resource.ris_buf)
            .write(rtxdi_resource.ris_light_data_buf)
            .read(reproject_tex)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, output.desc.extent);
        //spatial resampling
        rg::RenderPass::new_rt(
            rg.add_pass("rtxdi spatial resampling"),
            rhi::ShaderSource{ "/shaders/rtxdi/spatial_resampling_kernel.hlsl" },
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {}
            )
            .constants(sampling_constants)
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(rtxdi_resource.env_map_tex)
            .read(rtxdi_resource.env_map_pdf_tex)
            .read(rtxdi_resource.local_light_pdf_tex)
            .read(rtxdi_resource.light_data_buf)
            .read(rtxdi_resource.neighbor_offsets_buf)
            .read(rtxdi_resource.light_index_mapping_buf)
            .write(rtxdi_resource.light_reservoirs_buf)
            .write(rtxdi_resource.ris_buf)
            .write(rtxdi_resource.ris_light_data_buf)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, output.desc.extent);
        //shading samples
        rg::RenderPass::new_rt(
            rg.add_pass("rtxdi shade samples"),
            rhi::ShaderSource{ "/shaders/rtxdi/shade_samples_kenerl.hlsl" },
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {}
            )
            .constants(sampling_constants)
            .read(gbuffer_depth.gbuffer)
            .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
            .read(rtxdi_resource.env_map_tex)
            .read(rtxdi_resource.env_map_pdf_tex)
            .read(rtxdi_resource.local_light_pdf_tex)
            .read(rtxdi_resource.light_data_buf)
            .read(rtxdi_resource.neighbor_offsets_buf)
            .read(rtxdi_resource.light_index_mapping_buf)
            .write(rtxdi_resource.light_reservoirs_buf)
            .write(rtxdi_resource.ris_buf)
            .write(rtxdi_resource.ris_light_data_buf)
            .write(diffuse)
            .write(specular)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, output.desc.extent);
    }
    auto LightingPass::prepare_lights_for_sampling(
        rg::RenderGraph& rg,
        const GbufferDepth& gbuffer_depth,
        const rg::Handle<rhi::GpuTexture>& sky_cube,
        rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
        rg::Handle<rhi::GpuTexture>& output,
        const  rg::Handle<rhi::GpuTexture>& reproject_tex,
        rhi::DescriptorSet* bindless_descriptor_set,
        RtxDiResouceHandle& rtxdi_resource) -> void
    {
        auto render_width = output.desc.extent[0];
        auto render_height = output.desc.extent[1];
        auto fill_restirdi_constants = [&](ReSTIRDI_Parameters& params, const rtxdi::ReSTIRDIContext& restirDIContext, const RTXDI_LightBufferParameters& lightBufferParameters) {
            params.reservoirBufferParams = restirDIContext.GetReservoirBufferParameters();
            params.bufferIndices = restirDIContext.GetBufferIndices();
            params.initialSamplingParams = restirDIContext.GetInitialSamplingParameters();
            params.initialSamplingParams.environmentMapImportanceSampling = lightBufferParameters.environmentLightParams.lightPresent;
            if (!params.initialSamplingParams.environmentMapImportanceSampling)
                params.initialSamplingParams.numPrimaryEnvironmentSamples = 0;
            params.temporalResamplingParams = restirDIContext.GetTemporalResamplingParameters();
            params.spatialResamplingParams = restirDIContext.GetSpatialResamplingParameters();
            params.shadingParams = restirDIContext.GetShadingParameters();
        };
        auto fill_regir_constants = [](ReGIR_Parameters& params, const rtxdi::ReGIRContext& regirContext) {
            auto staticParams = regirContext.GetReGIRStaticParameters();
            auto dynamicParams = regirContext.GetReGIRDynamicParameters();
            auto gridParams = regirContext.GetReGIRGridCalculatedParameters();
            auto onionParams = regirContext.GetReGIROnionCalculatedParameters();

            params.gridParams.cellsX = staticParams.gridParameters.GridSize.x;
            params.gridParams.cellsY = staticParams.gridParameters.GridSize.y;
            params.gridParams.cellsZ = staticParams.gridParameters.GridSize.z;

            params.commonParams.numRegirBuildSamples = dynamicParams.regirNumBuildSamples;
            params.commonParams.risBufferOffset = regirContext.GetReGIRCellOffset();
            params.commonParams.lightsPerCell = staticParams.LightsPerCell;
            params.commonParams.centerX = dynamicParams.center.x;
            params.commonParams.centerY = dynamicParams.center.y;
            params.commonParams.centerZ = dynamicParams.center.z;
            params.commonParams.cellSize = (staticParams.Mode == rtxdi::ReGIRMode::Onion)
                ? dynamicParams.regirCellSize * 0.5f // Onion operates with radii, while "size" feels more like diameter
                : dynamicParams.regirCellSize;
            params.commonParams.localLightSamplingFallbackMode = static_cast<uint32_t>(dynamicParams.fallbackSamplingMode);
            params.commonParams.localLightPresamplingMode = static_cast<uint32_t>(dynamicParams.presamplingMode);
            params.commonParams.samplingJitter = std::max(0.f, dynamicParams.regirSamplingJitter * 2.f);
            params.onionParams.cubicRootFactor = onionParams.regirOnionCubicRootFactor;
            params.onionParams.linearFactor = onionParams.regirOnionLinearFactor;
            params.onionParams.numLayerGroups = uint32_t(onionParams.regirOnionLayers.size());

            assert(onionParams.regirOnionLayers.size() <= RTXDI_ONION_MAX_LAYER_GROUPS);
            for (int group = 0; group < int(onionParams.regirOnionLayers.size()); group++)
            {
                params.onionParams.layers[group] = onionParams.regirOnionLayers[group];
                params.onionParams.layers[group].innerRadius *= params.commonParams.cellSize;
                params.onionParams.layers[group].outerRadius *= params.commonParams.cellSize;
            }

            assert(onionParams.regirOnionRings.size() <= RTXDI_ONION_MAX_RINGS);
            for (int n = 0; n < int(onionParams.regirOnionRings.size()); n++)
            {
                params.onionParams.rings[n] = onionParams.regirOnionRings[n];
            }

            params.onionParams.cubicRootFactor = regirContext.GetReGIROnionCalculatedParameters().regirOnionCubicRootFactor;
        };
        auto fill_resampling_constants = [&](ResamplingConstants& constants, const rtxdi::ImportanceSamplingContext& isContext) {
            const RTXDI_LightBufferParameters& lightBufferParameters = isContext.GetLightBufferParameters();
            constants.lightBufferParams = isContext.GetLightBufferParameters();
            constants.localLightsRISBufferSegmentParams = isContext.GetLocalLightRISBufferSegmentParams();
            constants.environmentLightRISBufferSegmentParams = isContext.GetEnvironmentLightRISBufferSegmentParams();
            constants.runtimeParams = isContext.GetReSTIRDIContext().GetRuntimeParams();
            fill_restirdi_constants(constants.restirDI, isContext.GetReSTIRDIContext(), isContext.GetLightBufferParameters());
            fill_regir_constants(constants.regir, isContext.GetReGIRContext());
            //FillReSTIRGIConstants(constants.restirGI, isContext.GetReSTIRGIContext());
             constants.localLightPdfTextureSize = {rtxdi_resource.local_light_pdf_tex.desc.extent[0],rtxdi_resource.local_light_pdf_tex.desc.extent[1]};

            if (lightBufferParameters.environmentLightParams.lightPresent)
            {
                 constants.environmentPdfTextureSize = { rtxdi_resource.env_map_pdf_tex.desc.extent[0],rtxdi_resource.env_map_pdf_tex.desc.extent[1] };;
            }

            current_frame_output_reservoir = isContext.GetReSTIRDIContext().GetBufferIndices().shadingInputBufferIndex;
        };
        rtxdi::ReSTIRDIContext& restirDIContext = rtxdi_sampling_context->GetReSTIRDIContext();
        rtxdi::ReGIRContext& regirContext = rtxdi_sampling_context->GetReGIRContext();
        ResamplingConstants resampling_constants = {};
        fill_resampling_constants(resampling_constants, *rtxdi_sampling_context);
        auto& lightBufferParams = rtxdi_sampling_context->GetLightBufferParameters();
        if (rtxdi_sampling_context->IsLocalLightPowerRISEnabled() &&
            lightBufferParams.localLightBufferRegion.numLights > 0)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("presample_lights"),
                "/shaders/rtxdi/presample_lights.hlsl",
                {})
                .constants(resampling_constants)
                .read(gbuffer_depth.gbuffer)
                .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                .read(rtxdi_resource.env_map_tex)
                .read(rtxdi_resource.env_map_pdf_tex)
                .read(rtxdi_resource.local_light_pdf_tex)
                .read(rtxdi_resource.light_data_buf)
                .read(rtxdi_resource.neighbor_offsets_buf)
                .read(rtxdi_resource.light_index_mapping_buf)
                .write(rtxdi_resource.light_reservoirs_buf)
                .write(rtxdi_resource.ris_buf)
                .write(rtxdi_resource.ris_light_data_buf)
                .raw_descriptor_set(1,bindless_descriptor_set)
                .dispatch({ rtxdi_sampling_context->GetLocalLightRISBufferSegmentParams().tileSize ,
                    rtxdi_sampling_context->GetLocalLightRISBufferSegmentParams().tileCount,1});
        }

        if (lightBufferParams.environmentLightParams.lightPresent)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("presample_enviroment"),
                "/shaders/rtxdi/presample_enviroment.hlsl",
                {})
                .constants(resampling_constants)
                .read(gbuffer_depth.gbuffer)
                .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                .read(rtxdi_resource.env_map_tex)
                .read(rtxdi_resource.env_map_pdf_tex)
                .read(rtxdi_resource.local_light_pdf_tex)
                .read(rtxdi_resource.light_data_buf)
                .read(rtxdi_resource.neighbor_offsets_buf)
                .read(rtxdi_resource.light_index_mapping_buf)
                .write(rtxdi_resource.light_reservoirs_buf)
                .write(rtxdi_resource.ris_buf)
                .write(rtxdi_resource.ris_light_data_buf)
                .raw_descriptor_set(1,bindless_descriptor_set)
                .dispatch({ rtxdi_sampling_context->GetEnvironmentLightRISBufferSegmentParams().tileSize ,rtxdi_sampling_context->GetEnvironmentLightRISBufferSegmentParams().tileCount ,1});
        }
        if (rtxdi_sampling_context->IsReGIREnabled() &&
            lightBufferParams.localLightBufferRegion.numLights > 0)
        {
            auto get_regir_mode_macro = [](const rtxdi::ReGIRStaticParameters& regirStaticParams)->std::pair<std::string, std::string> {
                switch (regirStaticParams.Mode)
                {
                    case rtxdi::ReGIRMode::Disabled:
                        return {"RTXDI_REGIR_MODE","RTXDI_REGIR_DISABLED"};
                    case rtxdi::ReGIRMode::Grid:
                        return { "RTXDI_REGIR_MODE","RTXDI_REGIR_GRID" };
                    case rtxdi::ReGIRMode::Onion:
                        return { "RTXDI_REGIR_MODE","RTXDI_REGIR_ONION" };
                }
                return { "RTXDI_REGIR_MODE","RTXDI_REGIR_DISABLED" };
            };
            rtxdi::ReGIRStaticParameters regirStaticParams;
            std::vector<std::pair<std::string,std::string>> defines = {get_regir_mode_macro(regirStaticParams)};

            rg::RenderPass::new_compute(
                rg.add_pass("presample_regir"),
                "/shaders/rtxdi/presample_regir.hlsl",
                defines
                )
                .constants(resampling_constants)
                .read(gbuffer_depth.gbuffer)
                .read_aspect(gbuffer_depth.depth, rhi::ImageAspectFlags::DEPTH)
                .read(rtxdi_resource.env_map_tex)
                .read(rtxdi_resource.env_map_pdf_tex)
                .read(rtxdi_resource.local_light_pdf_tex)
                .read(rtxdi_resource.light_data_buf)
                .read(rtxdi_resource.neighbor_offsets_buf)
                .read(rtxdi_resource.light_index_mapping_buf)
                .write(rtxdi_resource.light_reservoirs_buf)
                .write(rtxdi_resource.ris_buf)
                .write(rtxdi_resource.ris_light_data_buf)
                .raw_descriptor_set(1,bindless_descriptor_set)
                .dispatch({ regirContext.GetReGIRLightSlotCount() ,1,1});
        }
        restir_lighting(
            rg,
            gbuffer_depth,
            sky_cube,
            tlas,
            output,
            reproject_tex,
            bindless_descriptor_set,
            rtxdi_resource,
            resampling_constants
        );
        last_frame_output_reservoir = current_frame_output_reservoir;
    }
 
    auto  LightingPass::sample_light_rendering(
                rg::RenderGraph& rg,
                const GbufferDepth& gbuffer_depth,
                const rg::Handle<rhi::GpuTexture>& sky_cube,
                rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
                rg::Handle<rhi::GpuTexture>& output,
                const rg::Handle<rhi::GpuTexture>& reproject_tex,
                rhi::DescriptorSet* bindless_descriptor_set)->void
    {
        auto rtxdi_resource = prepare_lights(
            rg,
            gbuffer_depth,
            sky_cube,
            tlas,
            output,
            bindless_descriptor_set
        );

        prepare_lights_for_sampling(
            rg,
            gbuffer_depth,
            sky_cube,
            tlas,
            output,
            reproject_tex,
            bindless_descriptor_set,
            rtxdi_resource
        );
    }
    
    auto LightingPass::gather_lights(
        std::vector<LightShaderData>& lights,
        const std::vector<TriangleLight>& triangle_lights)->void
    {
        lights.clear();
        std::vector<PrepareLightsTask> tasks;
        num_infinite_prim_lights = 0;
        num_finite_primLights = 0;
        num_importance_sampled_environment_lights = 0;
        num_emissive_triangles = 0;
        num_mesh_lights = 0;
        auto current_scene = renderer->current_scene;
        auto& registry = current_scene->get_registry();
        auto env_light_group = registry.group<Environment>(entt::get<maths::Transform>);
        auto dir_light_group = registry.group<DirectionalLightComponent>(entt::get<maths::Transform>);
        auto point_light_group = registry.group<PointLightComponent>(entt::get<maths::Transform>);
        auto spot_light_group = registry.group<SpotLightComponent>(entt::get<maths::Transform>);
        auto rect_light_group = registry.group<RectLightComponent>(entt::get<maths::Transform>);
        auto disk_light_group = registry.group<DiskLightComponent>(entt::get<maths::Transform>);
        auto cylinder_light_group = registry.group<CylinderLightComponent>(entt::get<maths::Transform>);
        light_buffer_offset = 0;

        for (auto light : triangle_lights)
        {
            size_t instanceHash = 0;
            diverse::hash_combine(instanceHash, light.instance_id);
            diverse::hash_combine(instanceHash, light.mesh_id);
            auto pOffset = instance_light_buffer_offsets.find(instanceHash);
            PrepareLightsTask task;
            task.instanceAndGeometryIndex = (light.instance_id << 12) | uint32_t(light.mesh_id & 0xfff);
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = light.triangle_count;
            task.previousLightBufferOffset = (pOffset != instance_light_buffer_offsets.end()) ? int(pOffset->second) : -1;

            // record the current offset of this instance for use on the next frame
            instance_light_buffer_offsets[instanceHash] = light_buffer_offset;

            light_buffer_offset += task.triangleCount;
            num_emissive_triangles += light.triangle_count;
            num_mesh_lights++;
            tasks.push_back(task);
        }
        for(auto entity : env_light_group)
		{
			const auto& [env,transform] = env_light_group.get<Environment, maths::Transform>(entity);
            if (!Entity(entity, current_scene).active()) continue;
			//env.get_light
            //num_importance_sampled_environment_lights++;
		}

        for (auto entity : dir_light_group)
        {
            const auto& [dir_light, transform] = dir_light_group.get<DirectionalLightComponent, maths::Transform>(entity);
            if(!Entity(entity,current_scene).active()) continue;

            auto pOffset = primitive_light_buffer_offsets.find(&dir_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&dir_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);
            LightShaderData light = {};
            dir_light.get_render_light_data(transform,&light);
            lights.push_back(light);
            num_infinite_prim_lights++;
        }

        for (auto entity : point_light_group)
		{
			const auto& [point_light, transform] = point_light_group.get<PointLightComponent, maths::Transform>(entity);
            if (!Entity(entity, current_scene).active()) continue;

            auto pOffset = primitive_light_buffer_offsets.find(&point_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&point_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);

            LightShaderData light = {};
			point_light.get_render_light_data(transform,&light);
			lights.push_back(light);
            num_finite_primLights++;
		}

        for (auto entity : spot_light_group)
        {
            const auto& [spot_light, transform] = spot_light_group.get<SpotLightComponent, maths::Transform>(entity);
            auto pOffset = primitive_light_buffer_offsets.find(&spot_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&spot_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);
            LightShaderData light = {};
			spot_light.get_render_light_data(transform,&light);
			lights.push_back(light);
            num_finite_primLights++;
		}

		for (auto entity : rect_light_group)
		{
			const auto& [rect_light, transform] = rect_light_group.get<RectLightComponent, maths::Transform>(entity);
            if (!Entity(entity, current_scene).active()) continue;
            auto pOffset = primitive_light_buffer_offsets.find(&rect_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&rect_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);

            LightShaderData light = {};
			rect_light.get_render_light_data(transform,&light);
			lights.push_back(light);
            num_finite_primLights++;
		}

        for (auto entity : cylinder_light_group)
        {
            const auto& [rect_light, transform] = cylinder_light_group.get<CylinderLightComponent, maths::Transform>(entity);
            if (!Entity(entity, current_scene).active()) continue;
            auto pOffset = primitive_light_buffer_offsets.find(&rect_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&rect_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);

            LightShaderData light = {};
            rect_light.get_render_light_data(transform, &light);
            lights.push_back(light);
            num_finite_primLights++;
        }

        for (auto entity : disk_light_group)
        {
            const auto& [rect_light, transform] = disk_light_group.get<DiskLightComponent, maths::Transform>(entity);
            if (!Entity(entity, current_scene).active()) continue;
            auto pOffset = primitive_light_buffer_offsets.find(&rect_light);

            PrepareLightsTask task;
            task.instanceAndGeometryIndex = TASK_PRIMITIVE_LIGHT_BIT | uint32_t(lights.size());
            task.lightBufferOffset = light_buffer_offset;
            task.triangleCount = 1; // technically zero, but we need to allocate 1 thread in the grid to process this light
            task.previousLightBufferOffset = (pOffset != primitive_light_buffer_offsets.end()) ? pOffset->second : -1;
            primitive_light_buffer_offsets[&rect_light] = light_buffer_offset;
            light_buffer_offset += task.triangleCount;
            tasks.push_back(task);

            LightShaderData light = {};
            rect_light.get_render_light_data(transform, &light);
            lights.push_back(light);
            num_finite_primLights++;
        }

        rhi_task_buffer->copy_from(g_device,(u8*)tasks.data(), tasks.size() * sizeof(PrepareLightsTask),0);
    }    
}