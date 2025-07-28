#pragma once
#include "drs_rg/simple_pass.h"
#include "utility/thread_pool.h"
#include "maths/maths_utils.h"
namespace diverse
{
    struct SortConstants
    {
        u32 numKeys;                        // The number of keys to sort
        u32 radixShift;                     // The radix shift value for the current pass
        u32 threadBlocks;                   // threadBlocks
        u32 padding0;                       // Padding - unused
    };

    //The size of a threadblock partition in the sort
    const uint DEVICE_RADIX_SORT_PARTITION_SIZE = 3840;

    //The size of our radix in bits
    const uint DEVICE_RADIX_SORT_BITS = 8;

    //Number of digits in our radix, 1 << DEVICE_RADIX_SORT_BITS
    const uint DEVICE_RADIX_SORT_RADIX = 256;

    //Number of sorting passes required to sort a 32bit key, KEY_BITS / DEVICE_RADIX_SORT_BITS
    const uint DEVICE_RADIX_SORT_PASSES = 4;


	auto gpu_sort(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& keys_src,
        rg::Handle<rhi::GpuBuffer>& values_src,
        u32 count)->std::pair<rg::Handle<rhi::GpuBuffer>,rg::Handle<rhi::GpuBuffer>>
	{
        SortConstants constants;
        constants.numKeys = count;
        constants.threadBlocks = maths::divide_rounding_up(count, DEVICE_RADIX_SORT_PARTITION_SIZE);

        //setup overall constants
        uint scratchBufferSize = maths::divide_rounding_up(count, DEVICE_RADIX_SORT_PARTITION_SIZE) * DEVICE_RADIX_SORT_RADIX;
        uint reducedScratchBufferSize = DEVICE_RADIX_SORT_RADIX * DEVICE_RADIX_SORT_PASSES;

        auto altBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(count * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER), "sortkey");
        auto altPayloadBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(count * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER),"sortvalue");
        auto passHistBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(scratchBufferSize * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER),"passhistbuf");
        auto globalHistBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(reducedScratchBufferSize * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER),"globalhist");

        auto dstKeyBuffer = altBuffer;
        auto dstPayloadBuffer = altPayloadBuffer;

        rg::RenderPass::new_compute(
            rg.add_pass("gpu_sort_init"), "/shaders/gpu_sort/init_radix_sort.hlsl")
            .write(globalHistBuffer)
            .dispatch({ 1 * 1024,1,1 });
        // Execute the sort algorithm in 8-bit increments
        for (constants.radixShift = 0; constants.radixShift < 32; constants.radixShift += DEVICE_RADIX_SORT_BITS)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_unsweep"), "/shaders/gpu_sort/unsweep.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .dispatch({ constants.threadBlocks * 128,1,1 });

            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_scan"), "/shaders/gpu_sort/scan.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .dispatch({ (int)DEVICE_RADIX_SORT_RADIX * 128,1,1 });

            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_down_sweep"), "/shaders/gpu_sort/down_sweep.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .dispatch({ constants.threadBlocks * 256,1,1 });

           std::swap(keys_src, dstKeyBuffer);
           std::swap(values_src, dstPayloadBuffer);
        }

        return { keys_src, values_src };
	}

    auto gpu_sort_indirect(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& keys_src,
        rg::Handle<rhi::GpuBuffer>& values_src,
        const rg::Handle<rhi::GpuBuffer>& count_buffer)->std::pair<rg::Handle<rhi::GpuBuffer>, rg::Handle<rhi::GpuBuffer>>
    {
        SortConstants constants;
        u32 const count = (u32)(keys_src.desc.size >> 2);
        constants.numKeys = count;
        constants.threadBlocks = maths::divide_rounding_up(count, DEVICE_RADIX_SORT_PARTITION_SIZE);

        //setup overall constants
        uint scratchBufferSize = maths::divide_rounding_up(count, DEVICE_RADIX_SORT_PARTITION_SIZE) * DEVICE_RADIX_SORT_RADIX;
        uint reducedScratchBufferSize = DEVICE_RADIX_SORT_RADIX * DEVICE_RADIX_SORT_PASSES;

        auto altBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(keys_src.desc.size, rhi::BufferUsageFlags::STORAGE_BUFFER), "sortkey");
        auto altPayloadBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(values_src.desc.size, rhi::BufferUsageFlags::STORAGE_BUFFER),"sortvalue");
        auto passHistBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(scratchBufferSize * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER),"passhistbuf");
        auto globalHistBuffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(reducedScratchBufferSize * sizeof(u32), rhi::BufferUsageFlags::STORAGE_BUFFER),"globalhist");

        auto args_buffer = rg.create<rhi::GpuBuffer>(rhi::GpuBufferDesc::new_gpu_only(sizeof(glm::ivec4) * 2, rhi::BufferUsageFlags::STORAGE_BUFFER | rhi::BufferUsageFlags::INDIRECT_BUFFER));
        
        auto dstKeyBuffer = altBuffer;
        auto dstPayloadBuffer = altPayloadBuffer;

        rg::RenderPass::new_compute(
            rg.add_pass("gpu sort dispatch"), "/shaders/gpu_sort/dispatch_args.hlsl")
            .write(args_buffer)
            .read(count_buffer)
            .dispatch({ 1,1,1 });

        rg::RenderPass::new_compute(
            rg.add_pass("gpu_sort_init"), "/shaders/gpu_sort/init_radix_sort.hlsl")
            .write(globalHistBuffer)
            .dispatch({ 1 * 1024,1,1 });
        // Execute the sort algorithm in 8-bit increments
        for (constants.radixShift = 0; constants.radixShift < 32; constants.radixShift += DEVICE_RADIX_SORT_BITS)
        {
            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_unsweep"), "/shaders/gpu_sort/unsweep_indirect.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .read(count_buffer)
                .dispatch_indirect(args_buffer,0);

            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_scan"), "/shaders/gpu_sort/scan_indirect.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .read(count_buffer)
                .dispatch({ (int)DEVICE_RADIX_SORT_RADIX * 128,1,1 });

            rg::RenderPass::new_compute(
                rg.add_pass("gpu_sort_down_sweep"), "/shaders/gpu_sort/down_sweep_indirect.hlsl")
                .constants(constants)
                .read(keys_src)
                .write(dstKeyBuffer)
                .read(values_src)
                .write(dstPayloadBuffer)
                .write(globalHistBuffer)
                .write(passHistBuffer)
                .read(count_buffer)
                .dispatch_indirect(args_buffer,16);

           std::swap(keys_src, dstKeyBuffer);
           std::swap(values_src, dstPayloadBuffer);
        }

        return { keys_src, values_src };
    }
}