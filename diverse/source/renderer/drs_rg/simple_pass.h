#pragma once
#include "pass_api.h"
#include "pass_builder.h"

namespace diverse
{
    namespace rg
    {
        struct ConstBlob
        {
            virtual auto push(rhi::DynamicConstants* dynamic_constants)->uint32 = 0;
        };

        template<typename T>
        struct ConstantData : public ConstBlob
        {
            T value;

            ConstantData(T const& t)
            : value(t)
            {}
            auto push(rhi::DynamicConstants* dynamic_constants) -> uint32 override
            {
               return dynamic_constants->push(value);
            }

            static auto from(T const& t) -> ConstantData<T>*
            {
                //TODO: vec switch case
                ConstantData<T> data(t);
                return frame_alloctor().allocate(data);
            }
        };

        template<typename T>
        struct ConstantVecData : public ConstBlob
        {
            T value;

            ConstantVecData(T const& t)
                : value(t)
            {}
            auto push(rhi::DynamicConstants* dynamic_constants) -> uint32 override
            {
                return dynamic_constants->push_from_vec(value);
            }

            static auto from(T const& t) -> ConstantVecData<T>*
            {
                //TODO: vec switch case
                ConstantVecData<T> data(t);
                return frame_alloctor().allocate(data);
            }
        };

        template<typename RgPipelineHandle>
        struct SimpleRenderPassState
        {
            RgPipelineHandle pipeline;
            std::vector<RenderPassBinding> bindings;
            std::vector<std::pair<uint32, ConstBlob*>> const_blobs;
            std::vector<std::pair<uint32, rhi::DescriptorSet*>> descriptor_sets;

            auto patch_const_blobs(RenderPassApi& api)->void
            {
                auto dynamic_constants = api.dynamic_constants();
                for (auto& [binding_idx, blob] : const_blobs)
                {
                    auto dynamic_constants_offset = blob->push(dynamic_constants);
                    switch (bindings[binding_idx].ty)
                    {
                    case RenderPassBinding::Type::DynamicConstants:
                    case RenderPassBinding::Type::DynamicConstantsStorageBuffer:
                    {
                        bindings[binding_idx].value = dynamic_constants_offset;
                    }
                        break;
                    default:
                        break;
                    }
                }
            }

            auto create_pipeline_binding() -> RenderPassPipelineBinding<RgPipelineHandle>
            {
                auto res = RenderPassPipelineBinding< RgPipelineHandle>::from(pipeline)
                    .descriptor_set(0, &bindings);

                for (auto& [set_idx, binding] : this->descriptor_sets)
                {
                    res = res.raw_descriptor_set(set_idx, binding);
                }
                return res;
            }
        };


        template<typename RgPipelineHandle>
        requires std::same_as<RgPipelineHandle, RgComputePipelineHandle>
            || std::same_as<RgPipelineHandle, RgRtPipelineHandle>
            || std::same_as<RgPipelineHandle, RgRasterPipelineHandle>
        struct SimpleRenderPass
        {
            PassBuilder pass;
            SimpleRenderPassState<RgPipelineHandle> state;
            SimpleRenderPass(PassBuilder&& builder, SimpleRenderPassState<RgPipelineHandle>&& sta)
            : pass(std::move(builder)), state(sta)
            {
            }

            SimpleRenderPass(const SimpleRenderPass& pass) = delete;

            //~SimpleRenderPass()
            //{
            //    pass.rg.record_pass(std::move(pass.pass));
            //}
            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource> &&
                         std::derived_from<Ref<Res,GpuSrv>, BindRgRef>
            auto read(const Handle<Res>& handle) -> SimpleRenderPass&
            {
                auto handle_ref = pass.read(
                    handle,
                    rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
                );

                state.bindings.push_back(handle_ref.bind());
                return *this;
            }

            auto read_array(const std::vector<Handle<rhi::GpuTexture>>& handles) -> SimpleRenderPass&
            {
        	    assert(!handles.empty());

        	    std::vector<Ref<rhi::GpuTexture, GpuSrv>> handle_refs;
        	    std::vector<RenderPassImageBinding> imgs_binding;
        	    std::transform(handles.begin(), handles.end(), [&](const Handle<rhi::GpuTexture>& handle) {
        		    //handle_refs.push_back({handle, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer});
                    handle_refs.push_back(this->pass.read(handle, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer));
        		    imgs_binding.push_back({handle_refs.back().handle, rhi::GpuTextureViewDesc() , rhi::ImageLayout::IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        	    });

        	    state.bindings.push_back(RenderPassBinding::ImageArray(imgs_binding));

        	    return *this;
            }

            auto read_view(const Handle<rhi::GpuTexture>& handle, rhi::GpuTextureViewDesc view_desc) -> SimpleRenderPass&
            {
    	        auto handle_ref = this->pass.read(
    		        handle,
    		        rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
    		        );

    	        state.bindings.push_back(handle_ref.bind_view(view_desc));
    	        return *this;
            }

            auto read_aspect(const Handle<rhi::GpuTexture>& handle, rhi::ImageAspectFlags aspect_mask) -> SimpleRenderPass&
            {
                auto handle_ref = this->pass.read(
                	handle,
                	rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
                	);

                state.bindings.push_back(handle_ref.bind_view(rhi::GpuTextureViewDesc().with_aspect_mask(aspect_mask)));
                return *this;
            }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource> &&
                std::derived_from<Ref<Res, GpuUav>, BindRgRef>
            auto write(Handle<Res>& handle) -> SimpleRenderPass&
            {
                auto handle_ref = pass.write(
                    handle,
                    rhi::AccessType::AnyShaderWrite
                );

                state.bindings.push_back(handle_ref.bind());
                return *this;
            }
            
            // auto write_aspect(Handle<rhi::GpuTexture>& handle, rhi::ImageAspectFlags aspect_mask) -> SimpleRenderPass&
            // {
            //     auto handle_ref = pass.write(
            //         handle,
            //         rhi::AccessType::AnyShaderWrite
            //     );

            //     state.bindings.push_back(handle_ref.bind_view(rhi::GpuTextureViewDesc().with_aspect_mask(aspect_mask)));
            //     return *this;
            // }

            template<typename Res>
                requires std::derived_from<Res, rhi::GpuResource> &&
                std::derived_from<Ref<Res, GpuUav>, BindRgRef>
            auto write_no_sync(Handle<Res>& handle) -> SimpleRenderPass&
            {   
                auto handle_ref = pass.write_no_sync(
                    handle,
                    rhi::AccessType::AnyShaderWrite
                );

                state.bindings.push_back(handle_ref.bind());
                return *this;
            }

            auto write_view(Handle<rhi::GpuTexture>& handle,const rhi::GpuTextureViewDesc& view_desc) -> SimpleRenderPass&
            {
            	auto handle_ref = this->pass.write(handle, rhi::AccessType::AnyShaderWrite);
            	state.bindings.push_back(handle_ref.bind_view(view_desc));

            	return *this;
            }

            template<typename T>
            auto constants(T const& t) -> SimpleRenderPass&
            {
                auto binding_idx = state.bindings.size();
                state.bindings.push_back(RenderPassBinding::DynamicConstants(0));
                state.const_blobs.push_back({ binding_idx , ConstantData<T>::from(t)});

                return *this;
            }

            template<typename T>
            auto dynamic_storage_buffer(T const& t) -> SimpleRenderPass&
            {
                auto binding_idx = state.bindings.size();
                state.bindings.push_back(RenderPassBinding::DynamicConstantsStorageBuffer(0));
                state.const_blobs.push_back({ binding_idx , ConstantData<T>::from(t) });

                return *this;
            }
            template<typename T>
            auto dynamic_storage_buffer_vec(const std::vector<T>& constants) -> SimpleRenderPass&
            {
                auto binding_idx = state.bindings.size();
                state.bindings.push_back(RenderPassBinding::DynamicConstantsStorageBuffer(0));
                state.const_blobs.push_back({ binding_idx , ConstantVecData<T>::from(constants) });

                return *this;
            }

            auto raw_descriptor_set(u32 set_idx,rhi::DescriptorSet* set) -> SimpleRenderPass&
            {
                state.descriptor_sets.push_back({set_idx, set});
                return *this;
            }

            template<typename Binding>
            auto bind(const Binding& binding) -> SimpleRenderPass&
            {
                return binding.bind(*this);
            }

            template<typename Binding>
            auto bind(Binding& binding) -> SimpleRenderPass&
            {
                return binding.bind(*this);
            }


            auto dispatch(const std::array<u32, 3>& extent) ->void
            {
                pass.render([r_state = std::move(this->state), extent](RenderPassApi& api)mutable {
                    r_state.patch_const_blobs(api);

                	auto pipeline = api.bind_compute_pipeline(r_state.create_pipeline_binding());

                	pipeline.dispatch(extent);
                });
                pass.rg->record_pass(std::move(pass.pass));
            }

            auto dispatch_indirect(const Handle<rhi::GpuBuffer>& args_buffer, uint64 args_buffer_offset) -> void
            {
                auto args_buffer_ref = this->pass.read(args_buffer, rhi::AccessType::IndirectBuffer);

    	        this->pass.render([r_state = std::move(this->state), args_buffer_ref = std::move(args_buffer_ref), args_buffer_offset](RenderPassApi& api)mutable {
                    r_state.patch_const_blobs(api);

    		        auto pipeline = api.bind_compute_pipeline(r_state.create_pipeline_binding());

    		        pipeline.dispatch_indirect(std::move(args_buffer_ref), args_buffer_offset);
    	        });
                pass.rg->record_pass(std::move(pass.pass));
            }

            auto dispatch_draw_instanced(u32 vertex_count, u32 instance_count)->void
            {
                pass.render([r_state = std::move(this->state), vertex_count, instance_count](RenderPassApi& api)mutable {
                    r_state.patch_const_blobs(api);

                    //api.begin_render_pass(
                    //    *gsplat_render_pass,
                    //    { width, height },
                    //    {
                    //        std::pair{color_ref,rhi::GpuTextureViewDesc()},
                    //    },
                    //    std::pair{ depth_ref, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                    //);

                    //api.set_default_view_and_scissor({ width,height });
                    auto pipeline = api.bind_raster_pipeline(r_state.create_pipeline_binding());
                    pipeline.dispatch_draw_instanced(vertex_count, instance_count);
                });
            }

            auto dispatch_indirect_draw_instanced(const Handle<rhi::GpuBuffer>& args_buffer, uint64 args_buffer_offset) -> void
            {

            }

            auto trace_rays(const Handle<rhi::GpuRayTracingAcceleration>& tlas, std::array<uint32, 3> extent)->void
            {
                auto tlas_ref = this->pass.read(tlas, rhi::AccessType::AnyShaderReadOther);
                this->pass.render([r_state = std::move(this->state), 
                    tlas_ref = std::move(tlas_ref), 
                    extent](RenderPassApi& api)mutable {
                    r_state.patch_const_blobs(api);
                    std::vector<RenderPassBinding> bindings = { tlas_ref.bind() };
                    auto pipeline = api.bind_ray_tracing_pipeline(r_state
                        .create_pipeline_binding()
                        .descriptor_set(3, &bindings));

                    pipeline.trace_rays(extent);
                    });

                pass.rg->record_pass(std::move(pass.pass));
            }
            auto trace_rays_indirect(const Handle<rhi::GpuRayTracingAcceleration>& tlas,
                const Handle<rhi::GpuBuffer>& args_buffer,
                uint64 args_buffer_offset)->void
            {
                auto args_buffer_ref = this->pass.read(args_buffer, rhi::AccessType::IndirectBuffer);
                auto tlas_ref = this->pass.read(tlas, rhi::AccessType::AnyShaderReadOther);
                this->pass.render([r_state = std::move(this->state), 
                        args_buffer_ref = std::move(args_buffer_ref), 
                        tlas_ref = std::move(tlas_ref),
                        args_buffer_offset](RenderPassApi& api) mutable {
                    r_state.patch_const_blobs(api);
                    std::vector<RenderPassBinding> bindings = { tlas_ref.bind() };
                    auto pipeline = api.bind_ray_tracing_pipeline(
                        r_state
                        .create_pipeline_binding()
                        .descriptor_set(3, &bindings)
                        );

                    pipeline.trace_rays_indirect(args_buffer_ref, args_buffer_offset);
                });

                pass.rg->record_pass(std::move(pass.pass));
            }

         /*   auto draw(std::function<void(RenderPassApi&)>&& fn) -> void
			{
				pass.render([r_state = std::move(this->state), extent](RenderPassApi& api)mutable {
		            
				});
				pass.rg->record_pass(std::move(pass.pass));
			}*/
        };

        template<typename RgPipelineHandle>
        struct BindMutToSimpleRenderPass
        {
            virtual auto bind(SimpleRenderPass<RgPipelineHandle>& pass) -> SimpleRenderPass<RgPipelineHandle> & = 0;
            virtual auto bind(SimpleRenderPass<RgPipelineHandle>& pass)const -> SimpleRenderPass<RgPipelineHandle> & = 0;
        };

        //using RasterRenderPass = SimpleRenderPass<RgRasterPipelineHandle>;

        struct RenderPass
        {
            static auto new_compute(PassBuilder&& pass, const std::string& pipeline_path,const std::vector<std::pair<std::string, std::string>>& defines = {}) -> SimpleRenderPass<RgComputePipelineHandle>;
            static auto new_compute(PassBuilder&& pass, rhi::ComputePipelineDesc&& desc)->SimpleRenderPass<RgComputePipelineHandle>;
            static auto new_rt(PassBuilder&& pass,
                rhi::ShaderSource rgen,
                const std::vector<rhi::ShaderSource>& miss,
                const std::vector<rhi::ShaderSource>& hit) -> SimpleRenderPass<RgRtPipelineHandle>;

            static auto new_raster(PassBuilder&& pass, 
                rhi::RasterPipelineDesc&& desc,
                const rhi::ShaderSource& vertex,
                const rhi::ShaderSource& fragment,
                const std::vector<std::pair<std::string, std::string>>& defines = {},
                const std::optional<rhi::ShaderSource>& gemotry = {}) -> SimpleRenderPass<RgRasterPipelineHandle>;
        };

        struct RasterPass
        {
            auto inline cull_mode(rhi::CullMode cull) -> RasterPass&
            {
                pipeline_desc.cull_mode = cull;
                return *this;
            }
            auto inline line_width(f32 line_w) -> RasterPass&
            {
                pipeline_desc.line_width = line_w;
                return *this;
            }
            auto inline depth_write(bool write) -> RasterPass&
            {
                pipeline_desc.depth_write = write;
                return *this;
            }

            auto inline discard_enable(bool val)->RasterPass&
            {
                pipeline_desc.discard_enable = val;
                return *this;
            }

            auto face_order(rhi::FrontFaceOrder order) -> RasterPass&
            {
                pipeline_desc.face_order = order;
                return *this;
            }
            auto inline depth_test(bool depthtest) -> RasterPass&
            {
                pipeline_desc.depth_test = depthtest;
                return *this;
            }

            auto inline vetex_attribute(bool vertex_attr)->RasterPass&
            {
                pipeline_desc.vextex_attribute = vertex_attr;
                return *this;
            }

            auto inline push_constants(u32 size) -> RasterPass&
            {
                pipeline_desc.push_constants_bytes = size;
                return *this;
            }
            auto inline primitive_type(rhi::PrimitiveTopType type) -> RasterPass&
            {
                pipeline_desc.primitive_type = type;
                return *this;
            }
            auto inline render_pass(const std::shared_ptr<rhi::RenderPass>& render_p) -> RasterPass&
            {
                pipeline_desc.with_render_pass(render_p);
                return *this;
            }

            auto inline blend_mode(rhi::BlendMode bm)->RasterPass&
            {
                pipeline_desc.with_blend_mode(bm);
                return *this;
            }
            rhi::RasterPipelineDesc pipeline_desc;

        };
    }
}
