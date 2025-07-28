#pragma once
#include "drs_rg/simple_pass.h"
#include "utility/thread_pool.h"

namespace diverse
{
    extern   auto parallel_scan(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& dst,
        rg::Handle<rhi::GpuBuffer>& src) -> void;

    extern   auto parallel_scan_indirect(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& dst,
        rg::Handle<rhi::GpuBuffer>& src,
        rg::Handle<rhi::GpuBuffer>& count) -> void;

    auto radix_sort(rg::RenderGraph& rg, 
        rg::Handle<rhi::GpuBuffer>& keys_src,
        rg::Handle<rhi::GpuBuffer>& keys_dst,
        rg::Handle<rhi::GpuBuffer>& values_src,
        rg::Handle<rhi::GpuBuffer>& values_dst)->void
    {
        uint32_t const group_size = 256;
        uint32_t const keys_per_thread = 4;
        uint32_t const num_bits_per_pass = 4;
        uint32_t const num_bins = (1 << num_bits_per_pass);
        uint32_t const keys_per_group = group_size * keys_per_thread;
        uint32_t const num_keys = (uint32_t)(keys_src.desc.size >> 2);
        uint32_t const num_groups = (num_keys + keys_per_group - 1) / keys_per_group;
        uint32_t const num_histogram_values = num_bins * num_groups;
        uint64_t scratch_buffer_size = (2 * num_keys + num_histogram_values + 5) << 2;

        auto scratch_keys = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_keys << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scratch_values = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_keys << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto group_histograms = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_histogram_values << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));

        for (u32 bitshift = 0, i = 0; bitshift < 32; bitshift += num_bits_per_pass, ++i)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("radix_sort_histogram"), "/shaders/radix_sort/histgram.hlsl")
                .write(i == 0 ? keys_src : (i & 1) != 0 ? scratch_keys : keys_dst)
                .write((i & 1) == 0 ? scratch_keys : keys_dst)
                .write(group_histograms)
                .constants(std::tuple{num_keys, bitshift })
                .dispatch({num_groups * group_size,1,1});

            parallel_scan(rg, group_histograms, group_histograms);
            rg::RenderPass::new_compute(
                rg.add_pass("radix_sort_scatter"), "/shaders/radix_sort/scatter.hlsl")
                .write(i == 0 ? keys_src : (i & 1) != 0 ? scratch_keys : keys_dst)
                .write((i & 1) == 0 ? scratch_keys : keys_dst)
                .write(i == 0 ? values_src : (i & 1) != 0 ? scratch_values : values_dst)
                .write((i & 1) == 0 ? scratch_values : values_dst)
                .write(group_histograms)
                .constants(std::tuple{num_keys, bitshift})
                .dispatch({ num_groups * group_size,1,1 });
        }
    }

    auto radix_sort_indirect(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& keys_src,
        rg::Handle<rhi::GpuBuffer>& keys_dst,
        rg::Handle<rhi::GpuBuffer>& values_src,
        rg::Handle<rhi::GpuBuffer>& values_dst,
        rg::Handle<rhi::GpuBuffer>& count,
        u32 num_pass) -> void
    {
        uint32_t const group_size = 256;
        uint32_t const keys_per_thread = 4;
        uint32_t const num_bits_per_pass = 4;
        uint32_t const num_bins = (1 << num_bits_per_pass);
        uint32_t const keys_per_group = group_size * keys_per_thread;
        uint32_t const num_keys = (uint32_t)(keys_src.desc.size >> 2);
        uint32_t const num_groups = (num_keys + keys_per_group - 1) / keys_per_group;
        uint32_t const num_histogram_values = num_bins * num_groups;
        uint64_t scratch_buffer_size = (2 * num_keys + num_histogram_values + 5) << 2;

        auto scratch_keys = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_keys << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scratch_values = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_keys << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto group_histograms = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_histogram_values << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER));
        auto scan_count_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4, rhi::BufferUsageFlags::STORAGE_BUFFER));

        for(u32 pass_id = 0; pass_id < num_pass; pass_id++)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("radix_sort_args"), "/shaders/radix_sort/dispatch_args.hlsl")
                .write(args_buffer)
                .write(scan_count_buffer)
                .write(count)
                .constants(pass_id)
                .dispatch({ 1,1,1 });

            for (u32 bitshift = 0, i = 0; bitshift < 32; bitshift += num_bits_per_pass, ++i)
            {
                rg::RenderPass::new_compute(
                    rg.add_pass("radix_sort_histogram"), "/shaders/radix_sort/histgram_indirect.hlsl")
                    .write(i == 0 ? keys_src : (i & 1) != 0 ? scratch_keys : keys_dst)
                    .write((i & 1) == 0 ? scratch_keys : keys_dst)
                    .write(group_histograms)
                    .write(count)
                    .constants(std::tuple{ pass_id, bitshift })
                    .dispatch_indirect(args_buffer,0);

                parallel_scan_indirect(rg, group_histograms, group_histograms, scan_count_buffer);

                rg::RenderPass::new_compute(
                    rg.add_pass("radix_sort_scatter"), "/shaders/radix_sort/scatter_indirect.hlsl")
                    .write(i == 0 ? keys_src : (i & 1) != 0 ? scratch_keys : keys_dst)
                    .write((i & 1) == 0 ? scratch_keys : keys_dst)
                    .write(i == 0 ? values_src : (i & 1) != 0 ? scratch_values : values_dst)
                    .write((i & 1) == 0 ? scratch_values : values_dst)
                    .write(group_histograms)
                    .write(count)
                    .constants(std::tuple{ pass_id, bitshift })
                    .dispatch_indirect(args_buffer,0);
            }
        }
    }
}