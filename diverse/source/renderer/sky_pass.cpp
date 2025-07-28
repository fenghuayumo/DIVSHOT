#include "sky_pass.h"

namespace diverse
{
    namespace sky 
    { 
        auto render_sky_cube(rg::RenderGraph& rg,const glm::vec4& color,int cubeResolution)->rg::Handle<rhi::GpuTexture>
        {
            u32 width = cubeResolution;
            auto sky_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_cube(PixelFormat::R16G16B16A16_Float,width), "sky_tex");

            rg::RenderPass::new_compute(rg.add_pass("sky cube"),
                "/shaders/sky/comp_cube.hlsl")
                .write_view(sky_tex,
                    rhi::GpuTextureViewDesc().with_view_type(rhi::TextureType::Tex2dArray))
                .constants(std::tuple{color,glm::uvec4(width, 0, 0, 0)})
                .dispatch({width, width, 6});
            return sky_tex;
        }

        auto convolve_cube(rg::RenderGraph& rg,const rg::Handle<rhi::GpuTexture>& input)->rg::Handle<rhi::GpuTexture>
        {
            u32 width = 16;
            auto sky_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_cube(PixelFormat::R16G16B16A16_Float,width), "convoled_sky_cube");

            rg::RenderPass::new_compute(rg.add_pass("convolve sky"),
                "/shaders/convolve_cube.hlsl")
                .read(input)
                .write_view(sky_tex,
                    rhi::GpuTextureViewDesc().with_view_type(rhi::TextureType::Tex2dArray)
                )
                .constants(width)
                .dispatch({width, width, 6});
            return sky_tex;
        }

        auto build_sky_env_map(
                rg::RenderGraph& rg,
                const rg::Handle<rhi::GpuTexture>& input,
                int envmap_width)
            ->std::tuple<rg::Handle<rhi::GpuTexture>,rg::Handle<rhi::GpuTexture>>
        {
            u32 width = envmap_width;
            auto sky_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R16G16B16A16_Float,{width,width/2}), "sky_tex");
            auto sky_pdf_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32_Float,{width,width/2}), "sky_pdf_tex");
            rg::RenderPass::new_compute(rg.add_pass("build_sky_env_map"),
                "/shaders/build_sky_env_map.hlsl")
                .read(input)
                .write(sky_pdf_tex)
                .write(sky_tex)
                .constants(sky_tex.desc.extent_inv_extent_2d())
                .dispatch({width, width/2, 1});
            return {sky_tex, sky_pdf_tex};
        }

        auto render_sky(rg::RenderGraph& rg, 
            const rg::Handle<rhi::GpuTexture>& convolved_sky_cube, 
            rg::Handle<rhi::GpuTexture>& color_img,
            rhi::DescriptorSet*  bindless_descriptor_set) -> rg::Handle<rhi::GpuTexture>
        {
            rg::RenderPass::new_compute(rg.add_pass("sky"), "/shaders/sky_render.hlsl")
                .write(color_img)
                .read(convolved_sky_cube)
                .constants(std::tuple{
                    color_img.desc.extent_inv_extent_2d(),
                    })
                .raw_descriptor_set(1, bindless_descriptor_set)
                .dispatch(color_img.desc.extent);

            return color_img;
        }
    }

    auto IblRenderer::render(rg::TemporalGraph& rg, const std::shared_ptr<rhi::GpuTexture>&  texture,const IblRenderParameter& param)-> std::optional<rg::Handle<rhi::GpuTexture>>
    {
        if (texture)
        {
            u32 width = 1024;
            auto cube_desc = rhi::GpuTextureDesc::new_cube(PixelFormat::R16G16B16A16_Float, width);
            auto cube_tex = rg.create<rhi::GpuTexture>(cube_desc, "ibl_cube");

            auto tex = rg.import_res<rhi::GpuTexture>(texture, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
            struct IblConstants{
                u32 width;
				f32 theta;
				f32 phi;
				u32 padding;
			};
            IblConstants cs = { width, param.theta, param.phi, 0u};
            rg::RenderPass::new_compute(rg.add_pass("ibl cube"), "/shaders/sky/ibl_cube.hlsl")
                .read(tex)
                .write_view(
                    cube_tex,
                    rhi::GpuTextureViewDesc().with_view_type(rhi::TextureType::Tex2dArray)
                )
                .constants(cs)
                .dispatch({ width, width, 6 });

            return cube_tex;
        }
        return {};
    }
}