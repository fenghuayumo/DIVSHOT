#pragma once
#include "core/base_type.h"
#include "assets/mesh.h"
#include "drs_rg/temporal.h"
#include "maths/transform.h"

namespace diverse
{

    class Camera;
    class GridRenderer
    {
    public:
        GridRenderer(rhi::GpuDevice* device);
        ~GridRenderer();

        auto init()->void;
        auto handle_resize(u32 width,u32 height)->void;
        auto render(rg::RenderGraph& rg,rg::Handle<rhi::GpuTexture>& color_img,rg::Handle<rhi::GpuTexture>& depth_img)->void;
        
        auto set_override_camera(Camera* overrideCamera, maths::Transform* overrideCameraTransform)->void;
    private:
        u32     current_buffer_id = 0;
        Mesh*   quad;

        float grid_res     = 1.0f;
        float grid_size    = 1.0f;
        float max_distance = 100000.0f;

        std::shared_ptr<rhi::RenderPass>   grid_render_pass;

        Camera* camera = nullptr;
        maths::Transform* camera_transform = nullptr;

        rhi::GpuDevice* device = nullptr;
    };
}