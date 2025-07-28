#include "drs_rg/simple_pass.h"
#include "irache.h"
#include "renderer/frame_constants.h"
#include "radiance_cache.h"
namespace diverse
{
    const f32 IRCACHE_GRID_CELL_DIAMETER = 0.16 * 0.125;
    const u32 IRCACHE_CASCADE_SIZE = 32;
    const u32 MAX_GRID_CELLS  = IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE * IRCACHE_CASCADE_SIZE * IRACHE_CASCADE_COUNT;
    const u32 MAX_ENTRIES = 1024 * 512;


    extern auto inclusive_prefix_scan_u32_1m(rg::RenderGraph& rg,
        const rg::Handle<rhi::GpuBuffer>& input_buf,
        rg::Handle<rhi::GpuBuffer>& output_buf) -> void;

    IracheRender::IracheRender()
        : initialized(false),
        grid_center(glm::vec3(0,0,0)),
        parity(0),
        enable_scroll(true)
    {
        for (auto cascade = 0; cascade < IRACHE_CASCADE_COUNT; cascade++)
        {
            prev_scroll[cascade] = glm::ivec3(0);
            cur_scroll[cascade] = glm::ivec3(0);
        }
    }

    auto IracheRender::update_eye_position(const glm::vec3& eye_pos) -> void
    {
        if(!enable_scroll)
            return;

        grid_center = eye_pos;

        for (auto cascade = 0; cascade < IRACHE_CASCADE_COUNT; cascade++)
        {
            auto cell_diameter = IRCACHE_GRID_CELL_DIAMETER * (1 << cascade) ;
            auto cascade_center = glm::ivec3(glm::floor(eye_pos / cell_diameter));
            auto cascade_origin = cascade_center - glm::ivec3(IRCACHE_CASCADE_SIZE / 2);

            prev_scroll[cascade] = cur_scroll[cascade];
            cur_scroll[cascade] = cascade_origin;
        }
    }
    auto IracheRender::constants()->std::array<SurfelCascadeConstants, IRACHE_CASCADE_COUNT>
    {
        std::array<SurfelCascadeConstants, IRACHE_CASCADE_COUNT> surfel_cascade_constants;
        for (auto c = 0; c < IRACHE_CASCADE_COUNT; c++)
        {
            const auto  cur_scroll = this->cur_scroll[c];
            const auto  prev_scroll = this->prev_scroll[c];
            const auto  scroll_amount = cur_scroll - prev_scroll;
            surfel_cascade_constants[c].origin = glm::ivec4(cur_scroll,0);
            surfel_cascade_constants[c].voxels_scrolled_this_frame = glm::ivec4(scroll_amount,0);
        }
        return surfel_cascade_constants;
    }
    auto IracheRender::prepare(rg::TemporalGraph& rg) -> IracheState
    {
        const u32 INDIRECTION_BUF_ELEM_COUNT  = 1024 * 1024;
        assert(INDIRECTION_BUF_ELEM_COUNT >= MAX_ENTRIES);

        IracheState state = {
            temporal_storage_buffer(rg, "ircache.meta_buf", sizeof(u32) * 8),
            temporal_storage_buffer(rg, "ircache.grid_meta_buf", sizeof(glm::ivec2) * MAX_GRID_CELLS),
            temporal_storage_buffer(rg, "ircache.grid_meta_buf2", sizeof(glm::ivec2) * MAX_GRID_CELLS),
            temporal_storage_buffer(rg, "ircache.entry_cell_buf", sizeof(u32) * MAX_ENTRIES),
            temporal_storage_buffer(rg, "ircache.spatial_buf", sizeof(glm::vec4) * MAX_ENTRIES),
            temporal_storage_buffer(rg, "ircache.irradiance_buf", 3 * sizeof(glm::vec4) * MAX_ENTRIES),

            temporal_storage_buffer(rg, "ircache.aux_buf",  4 * 16 * sizeof(glm::vec4) * MAX_ENTRIES),
            temporal_storage_buffer(rg, "ircache.life_buf", sizeof(u32) * MAX_ENTRIES),

            temporal_storage_buffer(rg, "ircache.pool_buf", sizeof(u32) * MAX_ENTRIES),
            temporal_storage_buffer(rg, "ircache.entry_indirection_buf", sizeof(u32) * INDIRECTION_BUF_ELEM_COUNT),

            temporal_storage_buffer(rg, "ircache.reposition_proposal_buf", sizeof(glm::vec4) * MAX_ENTRIES),
            temporal_storage_buffer(rg, "ircache.reposition_proposal_count_buf", sizeof(u32) * MAX_ENTRIES),

            false
        };
        if (1 == this->parity)
        {
            std::swap(state.ircache_grid_meta_buf, 
                state.ircache_grid_meta_buf2);
        }

        if (!initialized)
        {
            rg::RenderPass::new_compute(rg.add_pass("clear ircache pool"),
                "/shaders/ircache/clear_ircache_pool.hlsl"
                )
                .write(state.ircache_pool_buf)
                .write(state.ircache_life_buf)
                .dispatch({MAX_ENTRIES , 1, 1});
            initialized = true;
        }
        else
        {
            rg::RenderPass::new_compute(
                rg.add_pass("scroll cascades"),
                "/shaders/ircache/scroll_cascades.hlsl"
                )
                .read(state.ircache_grid_meta_buf)
                .write(state.ircache_grid_meta_buf2)
                .write(state.ircache_entry_cell_buf)
                .write(state.ircache_irradiance_buf)
                .write(state.ircache_life_buf)
                .write(state.ircache_pool_buf)
                .write(state.ircache_meta_buf)
                .dispatch({
                    IRCACHE_CASCADE_SIZE,
                    IRCACHE_CASCADE_SIZE,
                    IRCACHE_CASCADE_SIZE * IRACHE_CASCADE_COUNT
                });
            std::swap(state.ircache_grid_meta_buf, state.ircache_grid_meta_buf2);

            parity = (parity + 1) % 2;
        }
        auto indirect_args_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            (sizeof(u32) * 4) * 2,
            rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS | rhi::BufferUsageFlags::INDIRECT_BUFFER),
            "irache.indirect_args_buf");

        rg::RenderPass::new_compute(
            rg.add_pass("_ircache dispatch args"),
            "/shaders/ircache/prepare_age_dispatch_args.hlsl"
            )
            .write(state.ircache_meta_buf)
            .write(indirect_args_buf)
            .dispatch({1, 1, 1});

        auto entry_occupancy_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(sizeof(u32) * MAX_ENTRIES, 
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::STORAGE_BUFFER),
            "irache.entry_occupancy_buf");

        rg::RenderPass::new_compute(
            rg.add_pass("age ircache entries"),
            "/shaders/ircache/age_ircache_entries.hlsl"
            )
            .write(state.ircache_meta_buf)
            .write(state.ircache_grid_meta_buf)
            .write(state.ircache_entry_cell_buf)
            .write(state.ircache_life_buf)
            .write(state.ircache_pool_buf)
            .write(state.ircache_spatial_buf)
            .write(state.ircache_reposition_proposal_buf)
            .write(state.ircache_reposition_proposal_count_buf)
            .write(state.ircache_irradiance_buf)
            .write(entry_occupancy_buf)
            .dispatch_indirect(indirect_args_buf, 0);

        inclusive_prefix_scan_u32_1m(rg, entry_occupancy_buf, entry_occupancy_buf);

        rg::RenderPass::new_compute(
            rg.add_pass("ircache compact"),
            "/shaders/ircache/ircache_compact_entries.hlsl"
            )
            .write(state.ircache_meta_buf)
            .write(state.ircache_life_buf)
            .read(entry_occupancy_buf)
            .write(state.ircache_entry_indirection_buf)
            .dispatch_indirect(indirect_args_buf, 0);

        return state;
    }
    auto IracheRender::reset() -> void
    {
        parity = 0;
        initialized = false;
    }
    auto IracheState::trace_irradiance(
            rg::RenderGraph& rg, 
            const rg::Handle<rhi::GpuTexture>& sky_cube, 
            rhi::DescriptorSet* bindless_descriptor_set, 
            rg::Handle<rhi::GpuRayTracingAcceleration>& tlas,
            const radiance_cache::RadianceCacheState& rc_state) -> IracheIrradiancePendingSummation
    {
       auto indirect_args_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            (sizeof(u32) * 4) * 4,
            rhi::BufferUsageFlags::SHADER_DEVICE_ADDRESS | rhi::BufferUsageFlags::INDIRECT_BUFFER
            ));

        rg::RenderPass::new_compute(
            rg.add_pass("_ircache dispatch trace args"),
            "/shaders/ircache/prepare_trace_dispatch_args.hlsl"
            )
            .read(ircache_meta_buf)
            .write(indirect_args_buf)
            .dispatch({1, 1, 1});

        rg::RenderPass::new_compute(
            rg.add_pass("ircache reset"),
            "/shaders/ircache/reset_entry.hlsl"
            )
            .read(ircache_life_buf)
            .read(ircache_meta_buf)
            .read(ircache_irradiance_buf)
            .write(ircache_aux_buf)
            .read(ircache_entry_indirection_buf)
            .dispatch_indirect(indirect_args_buf, 16 * 2);

        rg::RenderPass::new_rt(
            rg.add_pass("ircache trace access"),
            rhi::ShaderSource{"/shaders/ircache/trace_accessibility.rgen.hlsl"},
            {
                // Duplicated because `rt.hlsl` hardcodes miss index to 1
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"},
            },
           {}
            )
            .read(ircache_spatial_buf)
            .read(ircache_life_buf)
            .write_no_sync(ircache_reposition_proposal_buf)
            .write_no_sync(ircache_meta_buf)
            .write_no_sync(ircache_aux_buf)
            .read(ircache_entry_indirection_buf)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays_indirect(tlas, indirect_args_buf, 16 * 1);

        rg::RenderPass::new_rt(
            rg.add_pass("ircache validate"),
            rhi::ShaderSource{"/shaders/ircache/ircache_validate.rgen.hlsl"},
            {
                rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
            )
            .read(ircache_spatial_buf)
            .read(sky_cube)
            .write_no_sync(ircache_grid_meta_buf)
            .read(ircache_life_buf)
            .write_no_sync(ircache_reposition_proposal_buf)
            .write_no_sync(ircache_reposition_proposal_count_buf)
            .bind(rc_state)
            .write_no_sync(ircache_meta_buf)
            .write_no_sync(ircache_aux_buf)
            .write_no_sync(ircache_pool_buf)
            .read(ircache_entry_indirection_buf)
            .write_no_sync(ircache_entry_cell_buf)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, {MAX_ENTRIES , 1, 1});

        rg::RenderPass::new_rt(
            rg.add_pass("ircache trace"),
            rhi::ShaderSource{"/shaders/ircache/trace_irradiance.rgen.hlsl"},
            {
                rhi::ShaderSource{"/shaders/rt/gbuffer.rmiss.hlsl"},
                rhi::ShaderSource{"/shaders/rt/shadow.rmiss.hlsl"}
            },
            {rhi::ShaderSource{"/shaders/rt/gbuffer.rchit.hlsl"}}
            )
            .read(ircache_spatial_buf)
            .read(sky_cube)
            .write_no_sync(ircache_grid_meta_buf)
            .read(ircache_life_buf)
            .write_no_sync(ircache_reposition_proposal_buf)
            .write_no_sync(ircache_reposition_proposal_count_buf)
            .bind(rc_state)
            .write_no_sync(ircache_meta_buf)
            .write_no_sync(ircache_aux_buf)
            .write_no_sync(ircache_pool_buf)
            .read(ircache_entry_indirection_buf)
            .write_no_sync(ircache_entry_cell_buf)
            .raw_descriptor_set(1, bindless_descriptor_set)
            .trace_rays(tlas, {MAX_ENTRIES, 1, 1});

        pending_irradiance_sum = true;

        return IracheIrradiancePendingSummation{ indirect_args_buf };
    }
    auto IracheState::sum_up_irradiance_for_sampling(rg::RenderGraph& rg, IracheIrradiancePendingSummation& pending) -> void
    {
        assert(pending_irradiance_sum);

        rg::RenderPass::new_compute(
            rg.add_pass("ircache sum"),
            "/shaders/ircache/sum_up_irradiance.hlsl"
            )
            .read(ircache_life_buf)
            .write(ircache_meta_buf)
            .write(ircache_irradiance_buf)
            .write(ircache_aux_buf)
            .read(ircache_entry_indirection_buf)
            .dispatch_indirect(pending.indirect_args_buf, 16 * 2);

        pending_irradiance_sum = false;
    }
}