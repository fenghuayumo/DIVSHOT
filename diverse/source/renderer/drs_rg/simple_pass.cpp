#include "simple_pass.h"

namespace diverse
{
	namespace rg
	{

		#define  RgPipelineHandleConstrain       template<typename RgPipelineHandle> \
			requires std::same_as<RgPipelineHandle, RgComputePipelineHandle>		 \
			|| std::same_as<RgPipelineHandle, RgRtPipelineHandle>					 \
			|| std::same_as<RgPipelineHandle, RgRasterPipelineHandle>                

		auto RenderPass::new_compute(PassBuilder&& pass, const std::string& pipeline_path, const std::vector<std::pair<std::string, std::string>>& defines) -> SimpleRenderPass<RgComputePipelineHandle>
		{
			auto pipeline = pass.register_compute_pipeline(pipeline_path, defines);
			return { std::move(pass), SimpleRenderPassState<RgComputePipelineHandle>{pipeline} };
		}
		auto RenderPass::new_compute(PassBuilder&& pass,rhi::ComputePipelineDesc&& desc) -> SimpleRenderPass<RgComputePipelineHandle>
		{
			auto pipeline = pass.register_compute_pipeline_with_desc(std::move(desc));
			return { std::move(pass), SimpleRenderPassState<RgComputePipelineHandle>{pipeline} };
		}
		auto RenderPass::new_rt(PassBuilder&& pass,
                rhi::ShaderSource rgen,
                const std::vector<rhi::ShaderSource>& miss,
				const std::vector<rhi::ShaderSource>& hit) -> SimpleRenderPass<RgRtPipelineHandle>
		{
			auto hit_count = hit.size();

			std::vector<rhi::PipelineShaderDesc>	shaders;
			shaders.push_back(
				rhi::PipelineShaderDesc()
				.with_stage(rhi::ShaderPipelineStage::RayGen)
				.with_shader_source(rgen)
			);

			for(auto& source : miss )
				shaders.push_back(
					rhi::PipelineShaderDesc()
					.with_stage(rhi::ShaderPipelineStage::RayMiss)
					.with_shader_source(source)
				);
			for(auto& source : hit)
				shaders.push_back(
					rhi::PipelineShaderDesc()
					.with_stage(rhi::ShaderPipelineStage::RayClosestHit)
					.with_shader_source(source)
				);
			if (hit_count > 0)
			{
                for (auto& source : { rhi::ShaderSource{"/shaders/rt/gbuffer.ahit.hlsl"}, rhi::ShaderSource{"/shaders/rt/shadow.ahit.hlsl"} })
				{
					shaders.push_back(
						rhi::PipelineShaderDesc()
						.with_stage(rhi::ShaderPipelineStage::RayAnyHit)
						.with_shader_source(source)
					);
				}
			}
			else
			{
                for (auto& source : { rhi::ShaderSource{"/shaders/rt/shadow.ahit.hlsl"}, rhi::ShaderSource{"/shaders/rt/shadow.ahit.hlsl"} })
				{
					shaders.push_back(
						rhi::PipelineShaderDesc()
						.with_stage(rhi::ShaderPipelineStage::RayAnyHit)
						.with_shader_source(source)
					);
				}
			}
			rhi::RayTracingPipelineDesc desc;
			desc.name = pass.pass.name;
			auto pipeline = pass.register_ray_tracing_pipeline(shaders, std::move(desc));

			return { std::move(pass), SimpleRenderPassState<RgRtPipelineHandle>{pipeline} };
		}

		auto RenderPass::new_raster(PassBuilder&& pass,
			rhi::RasterPipelineDesc&& pipeline_desc,
			const rhi::ShaderSource& vertex,
			const rhi::ShaderSource& fragment,
			const std::vector<std::pair<std::string, std::string>>& defines,
			const std::optional<rhi::ShaderSource>& geometry) -> SimpleRenderPass<RgRasterPipelineHandle>
		{
			std::vector<rhi::PipelineShaderDesc>	shaders;
			shaders.push_back(
				rhi::PipelineShaderDesc()
				.with_stage(rhi::ShaderPipelineStage::Vertex)
				.with_shader_source(vertex)
			);
			if (geometry)
			{
				shaders.push_back(
					rhi::PipelineShaderDesc()
					.with_stage(rhi::ShaderPipelineStage::Geometry)
					.with_shader_source(geometry.value())
				);
			}
			shaders.push_back(
				rhi::PipelineShaderDesc()
				.with_stage(rhi::ShaderPipelineStage::Pixel)
				.with_shader_source(fragment)
			);
			auto pipeline = pass.register_raster_pipeline(shaders, std::move(pipeline_desc));

			return { std::move(pass), SimpleRenderPassState<RgRasterPipelineHandle>{pipeline} };
		}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::read_array(const std::vector<Handle<rhi::GpuTexture>>& handles) -> SimpleRenderPass< RgPipelineHandle>&
		//{
		//	assert(!handles.empty());

		//	std::vector<Ref<rhi::GpuTexture, GpuSrv>> handle_refs;
		//	std::vector<RenderPassImageBinding> imgs_binding;
		//	std::transform(handles.begin(), handles.end(), [&](const Handle<rhi::GpuTexture>& handle) {
		//		handle_refs.push_back({handle, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer});
		//		imgs_binding.push_back({handle_refs.back(), rhi::GpuTextureViewDesc() , rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
		//	});
	
		//	state.bindings.push_back(RenderPassBinding::ImageArray(imgs_binding));

		//	return *this;
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::read_view(const Handle<rhi::GpuTexture>& handle, rhi::GpuTextureViewDesc view_desc) -> SimpleRenderPass<RgPipelineHandle>&
		//{
		//	auto handle_ref = this->pass.read(
		//		handle,
		//		rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
		//		);

		//	state.bindings.push_back(handle_ref.bind_view(view_desc));
		//	return *this;
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::read_aspect(const Handle<rhi::GpuTexture>& handle, rhi::ImageAspectFlags aspect_mask) -> SimpleRenderPass<RgPipelineHandle>&
		//{
		//	auto handle_ref = this->pass.read(
		//		handle,
		//		rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
		//		);

		//	state.bindings.push_back(handle_ref.bind_view(rhi::GpuTextureViewDesc().with_aspect_mask(aspect_mask)));
		//	return *this;
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::write_view(Handle<rhi::GpuTexture>& handle, rhi::GpuTextureViewDesc& view_desc) -> SimpleRenderPass<RgPipelineHandle>&
		//{
		//	auto handle_ref = this->pass.write(handle, rhi::AccessType::AnyShaderWrite);
		//	state.bindings.push_back(handle_ref.bind_view(view_desc));

		//	return *this;
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::dispatch(const std::array<u32, 3>& extent) -> void
		//{
		//	pass.render([&](RenderPassApi& api) {
		//		state.patch_const_blobs(api);

		//		auto pipeline = api.bind_compute_pipeline(state.create_pipeline_binding());

		//		pipeline.dispatch(extent);
		//	});
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::dispatch_indirect(const Handle<rhi::GpuBuffer>& args_buffer, uint64 args_buffer_offset)->void
		//{
		//	auto args_buffer_ref = this->pass.read(args_buffer, rhi::AccessType::IndirectBuffer);

		//	this->pass.render([&](RenderPassApi& api) {
		//		state.patch_const_blobs(api);

		//		auto pipeline = api.bind_compute_pipeline(state.create_pipeline_binding());

		//		pipeline.dispatch_indirect(args_buffer_ref, args_buffer_offset);
		//	});
		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::trace_rays(const Handle<rhi::RayTracingAcceleration>& tlas, std::array<uint32, 3> extent)->void
		//{

		//}

		//RgPipelineHandleConstrain
		//auto SimpleRenderPass<RgPipelineHandle>::trace_rays_indirect(const Handle<rhi::RayTracingAcceleration>& tlas,
		//	const Handle<rhi::GpuBuffer>& args_buffer,
		//	uint64 args_buffer_offset)->void
		//{

		//}


		//template<> struct SimpleRenderPass<RgComputePipelineHandle>;
		//template<> struct SimpleRenderPass<RgRtPipelineHandle>;
		//template<> struct SimpleRenderPass<RgRasterPipelineHandle>;
	}
}
