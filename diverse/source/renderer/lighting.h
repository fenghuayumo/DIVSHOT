#pragma once
#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
#include "scene/mesh_light.h"
#include "light.h"

namespace rtxdi
{
    class ImportanceSamplingContext;
}
namespace diverse
{
    struct IracheState;
    namespace radiance_cache
    {
       struct RadianceCacheState;
    }
    class LightComponent;
    struct PrepareLightsTask
    {
        uint instanceAndGeometryIndex; // low 12 bits are geometryIndex, mid 19 bits are instanceIndex, high bit is TASK_PRIMITIVE_LIGHT_BIT
        uint triangleCount;
        uint lightBufferOffset;
        int previousLightBufferOffset; // -1 means no previous data
    };

    struct RtxDiResouceHandle
    {
        rg::Handle<rhi::GpuTexture> env_map_tex;
        rg::Handle<rhi::GpuTexture> env_map_pdf_tex;
        rg::Handle<rhi::GpuTexture> local_light_pdf_tex;
        rg::Handle<rhi::GpuBuffer>  light_data_buf;
        rg::Handle<rhi::GpuBuffer>  neighbor_offsets_buf;
        rg::Handle<rhi::GpuBuffer>  light_index_mapping_buf;
        rg::Handle<rhi::GpuBuffer>  light_reservoirs_buf;
        rg::Handle<rhi::GpuBuffer>  ris_buf;
        rg::Handle<rhi::GpuBuffer>  ris_light_data_buf;
    };
    class LightingPass
    {
    public:
        LightingPass(struct DeferedRenderer* render);
        ~LightingPass();
        auto gather_lights(
            std::vector<LightShaderData>& lights,
            const std::vector<TriangleLight>& triangle_lights)->void;

        auto lighting_gbuffer(
            rg::RenderGraph& rg,
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
            u32 debug_shading_mode)->void;
        auto sample_light_rendering(
            rg::RenderGraph& rg,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            rg::Handle<rhi::GpuTexture>& output,
            const rg::Handle<rhi::GpuTexture>& reproject_tex,
            rhi::DescriptorSet* bindless_descriptor_set)->void;
    protected:
       auto prepare_lights(
            rg::RenderGraph& rg,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            rg::Handle<rhi::GpuTexture>& output,
            rhi::DescriptorSet* bindless_descriptor_set)-> RtxDiResouceHandle;
        auto prepare_lights_for_sampling(
            rg::RenderGraph& rg,
            const GbufferDepth& gbuffer_depth,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            rg::Handle<rhi::GpuTexture>& output,
            const rg::Handle<rhi::GpuTexture>& reprojection_tex,
            rhi::DescriptorSet* bindless_descriptor_set,
            RtxDiResouceHandle& rtxdi_resource)->void;
    private:
        std::shared_ptr<rhi::GpuBuffer>    neighbor_offsets_buf;
        struct DeferedRenderer* renderer;
        std::unique_ptr<rtxdi::ImportanceSamplingContext> rtxdi_sampling_context;
        std::unordered_map<const LightComponent*, uint32_t> primitive_light_buffer_offsets;
        std::unordered_map<size_t, uint32_t>    instance_light_buffer_offsets;
        u32 num_finite_primLights = 0;
        u32 num_infinite_prim_lights = 0;
        u32 num_importance_sampled_environment_lights = 0;
        u32 num_mesh_lights = 0;
        u32 num_emissive_triangles = 0;
        u32 light_buffer_offset = 0;
        std::shared_ptr<rhi::GpuBuffer> rhi_task_buffer;
        bool odd_frame = false;

        uint32_t last_frame_output_reservoir = 0;
        uint32_t current_frame_output_reservoir = 0;

    };
}