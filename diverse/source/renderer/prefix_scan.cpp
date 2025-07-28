#pragma once
#include "drs_rg/simple_pass.h"

namespace diverse
{
    auto inclusive_prefix_scan_u32_1m(rg::RenderGraph& rg,
                            const rg::Handle<rhi::GpuBuffer>& input_buf,
                            rg::Handle<rhi::GpuBuffer>& out_buf)->void
    {
        const u32 SEGMENT_SIZE = 1024;

        rg::RenderPass::new_compute(
            rg.add_pass("_prefix scan 1"),
            "/shaders/prefix_scan/inclusive_prefix_scan.hlsl"
            )
            .read(input_buf)
            .write(out_buf)
            .dispatch({SEGMENT_SIZE * SEGMENT_SIZE / 2, 1, 1}); // TODO: indirect

        auto segment_sum_buf = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(
            sizeof(u32) * SEGMENT_SIZE,
            rhi::BufferUsageFlags::TRANSFER_SRC |
            rhi::BufferUsageFlags::TRANSFER_DST |
            rhi::BufferUsageFlags::STORAGE_BUFFER
        ));
        rg::RenderPass::new_compute(
            rg.add_pass("_prefix scan 2"),
            "/shaders/prefix_scan/inclusive_prefix_scan_segments.hlsl"
            )
            .read(out_buf)
            .write(segment_sum_buf)
            .dispatch({SEGMENT_SIZE / 2, 1, 1}); // TODO: indirect

        rg::RenderPass::new_compute(
            rg.add_pass("_prefix scan merge"),
            "/shaders/prefix_scan/inclusive_prefix_scan_merge.hlsl"
            )
            .write(out_buf)
            .read(segment_sum_buf)
            .dispatch({SEGMENT_SIZE * SEGMENT_SIZE / 2, 1, 1}); // TODO: indirect
    }
}