#pragma once
#include "drs_rg/simple_pass.h"

namespace diverse
{

    enum OpType
    {
        kOpType_Min = 0,
        kOpType_Max,
        kOpType_Sum,

        kOpType_Count
    };

    // enum DataType
    // {
    //     kDataType_Int = 0,
    //     kDataType_Uint,
    //     kDataType_Float,

    //     kDataType_Count
    // };


    auto parallel_scan(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& dst,
		rg::Handle<rhi::GpuBuffer>& src)->void
	{
        u32 const group_size = 256;
        u32 const keys_per_thread = 4;
        u32 const keys_per_group = group_size * keys_per_thread;
        u32 const num_keys = (u32)(src.desc.size >> 2);
        u32 const num_groups_level_1 = (num_keys + keys_per_group - 1) / keys_per_group;
        u32 const num_groups_level_2 = (num_groups_level_1 + keys_per_group - 1) / keys_per_group;
        if(num_groups_level_2 > keys_per_group)
        {
            return;    
        }
        u64 scratch_buffer_size = (num_groups_level_1 + num_groups_level_2 + 10) << 2;
        auto scan_level_1_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_groups_level_1 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scan_level_2_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_groups_level_2 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto num_groups_level_1_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto num_groups_level_2_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scan_level_1_args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scan_level_2_args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        if (num_keys > keys_per_group)
        {
            rg::RenderPass::new_compute(rg.add_pass("reduce_kernel0"), "/shaders/radix_sort/block_reduce.hlsl")
                .write(src)
                .write(scan_level_1_buffer)
                .constants(num_keys)
               .dispatch({ num_groups_level_1 * group_size, 1, 1 });

            if (num_groups_level_1 > keys_per_group)
            {
                rg::RenderPass::new_compute(rg.add_pass("reduce_kernel1"), "/shaders/radix_sort/block_reduce.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_2_buffer)
                    .constants(num_groups_level_1)
                    .dispatch({ num_groups_level_2 * group_size, 1, 1 });

                rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan.hlsl")
                    .write(scan_level_2_buffer)
                    .write(scan_level_2_buffer)
                    .constants(num_groups_level_2)
                    .dispatch({ 1 * group_size, 1, 1 });
            }
 
            if (num_groups_level_1 > keys_per_group)
            {
                rg::RenderPass::new_compute(rg.add_pass("scan_add_kernel"), "/shaders/radix_sort/block_scan_add.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_1_buffer)
                    .write(scan_level_2_buffer)
                    .constants(num_groups_level_1)
                    .dispatch({ num_groups_level_2 * group_size, 1, 1 });
            }
            else
            {
                rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_1_buffer)
                    .constants(num_groups_level_1 )
                    .dispatch({ num_groups_level_2 * group_size, 1, 1 });
            }
        }
        if(num_keys > keys_per_group )
        { 
            rg::RenderPass::new_compute(rg.add_pass("scan_add_kernel"), "/shaders/radix_sort/block_scan_add.hlsl")
                .write(src)
                .write(dst)
                .write(scan_level_1_buffer)
                .constants(std::tuple{ num_keys })
                .dispatch({ num_groups_level_1 * group_size, 1, 1 });
        }
        else
        {
            rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan.hlsl")
                .write(src)
                .write(dst)
                .constants(std::tuple{ num_keys })
                .dispatch({ num_groups_level_1 * group_size, 1, 1 });
        }
	}

    auto parallel_scan_indirect(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& dst,
		rg::Handle<rhi::GpuBuffer>& src,
		rg::Handle<rhi::GpuBuffer>& count)->void
	{
        u32 const group_size = 256;
        u32 const keys_per_thread = 4;
        u32 const keys_per_group = group_size * keys_per_thread;
        u32 const num_keys = (u32)(src.desc.size >> 2);
        u32 const num_groups_level_1 = (num_keys + keys_per_group - 1) / keys_per_group;
        u32 const num_groups_level_2 = (num_groups_level_1 + keys_per_group - 1) / keys_per_group;
        if(num_groups_level_2 > keys_per_group)
        {
            return;    
        }
        u64 scratch_buffer_size = (num_groups_level_1 + num_groups_level_2 + 10) << 2;
        auto scan_level_1_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_groups_level_1 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scan_level_2_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(num_groups_level_2 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto num_groups_level_1_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto num_groups_level_2_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4, rhi::BufferUsageFlags::STORAGE_BUFFER));
        auto scan_level_1_args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER));
        auto scan_level_2_args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(4 << 2, rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER));

        rg::RenderPass::new_compute(rg.add_pass("scan_args"), "/shaders/radix_sort/scan_args_dispatch.hlsl")
            .write(count)
            .write(scan_level_1_args_buffer)
            .write(scan_level_2_args_buffer)
            .write(num_groups_level_1_buffer)
            .write(num_groups_level_2_buffer)
            .dispatch({ 1, 1, 1 });

        if (num_keys > keys_per_group)
        {
            rg::RenderPass::new_compute(rg.add_pass("reduce_kernel0"), "/shaders/radix_sort/block_reduce_indirect.hlsl")
                .write(src)
                .write(scan_level_1_buffer)
                .write(count)
               .dispatch_indirect(scan_level_1_args_buffer,0);

            if (num_groups_level_1 > keys_per_group)
            {
                rg::RenderPass::new_compute(rg.add_pass("reduce_kernel1"), "/shaders/radix_sort/block_reduce_indirect.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_2_buffer)
                    .write(num_groups_level_1_buffer)
                    .dispatch_indirect(scan_level_2_args_buffer,0);
                
                rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan_indirect.hlsl")
                .write(scan_level_2_buffer)
                .write(scan_level_2_buffer)
                .write(num_groups_level_2_buffer)
                .dispatch({ 1 * group_size, 1, 1 });
            }


            if (num_groups_level_1 > keys_per_group)
            {
                rg::RenderPass::new_compute(rg.add_pass("scan_add_kernel"), "/shaders/radix_sort/block_scan_add_indirect.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_1_buffer)
                    .write(scan_level_2_buffer)
                    .write(num_groups_level_1_buffer)
                    .dispatch_indirect(scan_level_2_args_buffer,0);
            }
            else
            {
                rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan_indirect.hlsl")
                    .write(scan_level_1_buffer)
                    .write(scan_level_1_buffer)
                    .write(num_groups_level_1_buffer)
                    .dispatch_indirect(scan_level_2_args_buffer, 0);
            }
        }
        if(num_keys > keys_per_group )
        { 
            rg::RenderPass::new_compute(rg.add_pass("scan_add_kernel"), "/shaders/radix_sort/block_scan_add_indirect.hlsl")
                .write(src)
                .write(dst)
                .write(scan_level_1_buffer)
                .write(count)
                .dispatch_indirect(scan_level_1_args_buffer, 0);
        }
        else
        {
            rg::RenderPass::new_compute(rg.add_pass("scan_kernel"), "/shaders/radix_sort/block_scan_indirect.hlsl")
                .write(src)
                .write(dst)
                .write(count)
                .dispatch_indirect(scan_level_1_args_buffer, 0);
        }
	}
    //auto scan_sum(rg::RenderGraph& rg,
	//	rg::Handle<rhi::GpuBuffer>& dst,
	//	rg::Handle<rhi::GpuBuffer>& src,
	//	const rg::Handle<rhi::GpuBuffer>& count)
	//{

	//}

	//auto scan_min(rg::RenderGraph& rg,
	//	rg::Handle<rhi::GpuBuffer>& dst,
	//	rg::Handle<rhi::GpuBuffer>& src,
	//	const rg::Handle<rhi::GpuBuffer>& count)
	//{

	//}
	//auto scan_max(rg::RenderGraph& rg,
	//	rg::Handle<rhi::GpuBuffer>& dst,
	//	rg::Handle<rhi::GpuBuffer>& src,
	//	const rg::Handle<rhi::GpuBuffer>& count)
	//{

	//}
}