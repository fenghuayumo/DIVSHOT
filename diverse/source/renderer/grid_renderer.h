#pragma once

#include "core/base_type.h"

#include "assets/cpu_assets.h"

#include "assets/gpu_assets.h"

#include "drs_rg/temporal.h"

#include "frame_snapshot.h"

#include "maths/transform.h"

#include <memory>



namespace diverse

{



    class GridRenderer

    {

    public:

        GridRenderer(rhi::GpuDevice* device);

        ~GridRenderer();



        auto init()->void;

        auto handle_resize(u32 width,u32 height)->void;

        auto render(rg::RenderGraph& rg,rg::Handle<rhi::GpuTexture>& color_img,rg::Handle<rhi::GpuTexture>& depth_img)->void;

        

        auto set_frame_params(const GridFrameParams& params)->void;

    private:

        u32     current_buffer_id = 0;

        std::shared_ptr<MeshAsset> quad;

        MeshGpu quad_gpu;



        float grid_res     = 1.0f;

        float grid_size    = 1.0f;

        float max_distance = 100000.0f;



        std::shared_ptr<rhi::RenderPass>   grid_render_pass;



        GridFrameParams frame_params;



        rhi::GpuDevice* device = nullptr;

    };

}

