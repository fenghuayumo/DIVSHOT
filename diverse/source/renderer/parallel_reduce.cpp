#pragma once
#include "drs_rg/simple_pass.h"

namespace diverse
{
	auto reduce_sum(rg::RenderGraph& rg,
		rg::Handle<rhi::GpuBuffer>& dst,
		rg::Handle<rhi::GpuBuffer>& src,
		const rg::Handle<rhi::GpuBuffer>& count)
	{

	}

	//auto reduce_max(rg::RenderGraph& rg,
	//	rg::Handle<rhi::GpuBuffer>& dst,
	//	rg::Handle<rhi::GpuBuffer>& src,
	//	const rg::Handle<rhi::GpuBuffer>& count)
	//{

	//}

	//auto reduce_min(rg::RenderGraph& rg,
	//	rg::Handle<rhi::GpuBuffer>& dst,
	//	rg::Handle<rhi::GpuBuffer>& src,
	//	const rg::Handle<rhi::GpuBuffer>& count)
	//{

	//}
}