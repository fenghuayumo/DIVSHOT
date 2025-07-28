#pragma once
#include "drs_rg/simple_pass.h"
#include "gbuffer_depth.h"
#include "renderer/frame_constants.h"

namespace diverse
{
    namespace radiance_cache
    {
        struct RadianceCacheState;
    }
    struct IracheIrradiancePendingSummation
    {
        rg::Handle<rhi::GpuBuffer>  indirect_args_buf;
    };
    struct IracheState
    {
        rg::Handle<rhi::GpuBuffer> ircache_meta_buf;

        rg::Handle<rhi::GpuBuffer> ircache_grid_meta_buf;
        rg::Handle<rhi::GpuBuffer> ircache_grid_meta_buf2;
        rg::Handle<rhi::GpuBuffer> ircache_entry_cell_buf;
        rg::Handle<rhi::GpuBuffer> ircache_spatial_buf;

        rg::Handle<rhi::GpuBuffer> ircache_irradiance_buf;
        rg::Handle<rhi::GpuBuffer> ircache_aux_buf;
        rg::Handle<rhi::GpuBuffer> ircache_life_buf;
        rg::Handle<rhi::GpuBuffer> ircache_pool_buf;
        rg::Handle<rhi::GpuBuffer> ircache_entry_indirection_buf;
        rg::Handle<rhi::GpuBuffer> ircache_reposition_proposal_buf;
        rg::Handle<rhi::GpuBuffer> ircache_reposition_proposal_count_buf;

        bool pending_irradiance_sum;

        template<typename RgPipelineHandle>
        auto bind(rg::SimpleRenderPass<RgPipelineHandle>& pass) -> rg::SimpleRenderPass<RgPipelineHandle>&
        {
            assert(!pending_irradiance_sum);

            return  pass.write_no_sync(ircache_meta_buf)
                        .write_no_sync(ircache_pool_buf)
                        .write_no_sync(ircache_reposition_proposal_buf)
                        .write_no_sync(ircache_reposition_proposal_count_buf)
                        .write_no_sync(ircache_grid_meta_buf)
                        .write_no_sync(ircache_entry_cell_buf)
                        .read(ircache_spatial_buf)
                        .read(ircache_irradiance_buf)
                        .write_no_sync(ircache_life_buf);
        }

        template<typename RgPipelineHandle>
        auto bind(rg::SimpleRenderPass<RgPipelineHandle>& pass)const -> rg::SimpleRenderPass<RgPipelineHandle>&
        {
            assert(!pending_irradiance_sum);

            return  pass.write_no_sync(ircache_meta_buf)
                        .write_no_sync(ircache_pool_buf)
                        .write_no_sync(ircache_reposition_proposal_buf)
                        .write_no_sync(ircache_reposition_proposal_count_buf)
                        .write_no_sync(ircache_grid_meta_buf)
                        .write_no_sync(ircache_entry_cell_buf)
                        .read(ircache_spatial_buf)
                        .read(ircache_irradiance_buf)
                        .write_no_sync(ircache_life_buf);
        }

        auto trace_irradiance(rg::RenderGraph& rg,
            const rg::Handle<rhi::GpuTexture>& sky_cube,
            rhi::DescriptorSet* bindless_descriptor_set,
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            const radiance_cache::RadianceCacheState& rc_state) -> IracheIrradiancePendingSummation;

        auto sum_up_irradiance_for_sampling(rg::RenderGraph& rg,
            IracheIrradiancePendingSummation& pending) -> void;
    };
    
    inline auto temporal_storage_buffer(rg::TemporalGraph& rg,
                        const char* name,
                        u32 size) -> rg::Handle<rhi::GpuBuffer>
    {
        return rg.get_or_create_temporal(
            name,
            rhi::GpuBufferDesc::new_gpu_only(size, rhi::BufferUsageFlags::STORAGE_BUFFER));
    }



    struct IracheRender
    {
    protected:
        bool initialized;
        glm::vec3 grid_center;
        std::array<glm::ivec3, IRACHE_CASCADE_COUNT> cur_scroll;
        std::array<glm::ivec3, IRACHE_CASCADE_COUNT> prev_scroll;

        u32 parity ;
    public:
        bool enable_scroll = false;

        IracheRender();
        auto update_eye_position(const glm::vec3& eye_pos)->void;
        auto constants()->std::array<SurfelCascadeConstants, IRACHE_CASCADE_COUNT>;
        auto prepare(rg::TemporalGraph& rg) -> IracheState;
        auto reset()->void;
        auto get_grid_center() -> glm::vec3
        {
            return grid_center;
        }
    };
}