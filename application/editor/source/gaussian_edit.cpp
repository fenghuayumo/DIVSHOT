#include "gaussian_edit.h"
#include "redo_undo_system.h"
#include <backend/drs_rhi/gpu_device.h>
#include <opencv2/opencv.hpp>
#include <utility/thread_pool.h>
#include <engine/application.h>
#include <scene/scene.h>
#include <renderer/defered_renderer.h>
#include <renderer/drs_rg/image_op.h>
#include <maths/maths_log.hpp>
namespace diverse
{
    auto process_selection(GaussianModel* splat,const EditSelectOpType& op, std::function<bool(int i)>&& pred)->void
    {
        if(!splat) return;
        parallel_for<size_t>(0, splat->position().size(), [&](size_t i){
            u32 state = splat->state()[i];
            if (state == DELETE_STATE || state == HIDE_STATE) {
                //state = NORMAL_STATE;
            }
            else{
                const auto result = pred(i);
                if (op == EditSelectOpType::Add)
                {
                    if (result) {
                        state |= SELECT_STATE;
                    }
                }
                else if (op == EditSelectOpType::Remove)
                {
                    if (result) {
                        state &= ~SELECT_STATE;
                    }
                }
                else if (op == EditSelectOpType::Set)
                {
                    if (result) {
                        state |= SELECT_STATE;
                    }
                    else {
                        state &= ~SELECT_STATE;
                    }
                }
                splat->state()[i] = state;
            }
        });
        splat->update_state();
    }

    GaussianEdit::GaussianEdit()
    {
        bdbox = maths::BoundingBox(glm::vec3(-1,-1,-1), glm::vec3(1,1,1));
        bdsphere = maths::BoundingSphere(glm::vec3(0,0,0), 2.0f);
    }

    GaussianEdit::~GaussianEdit()
    {
    }

    void GaussianEdit::set_mouse_pos(const glm::vec2& pos)
    {
        mouse_pos = pos;
    }
    glm::vec2 GaussianEdit::get_mouse_pos() const
    {
        return mouse_pos;
    }

    void GaussianEdit::set_edit_mode(EditMode mode)
    {
        edit_op = (edit_op & 0x0FFFFFFF) | (((int)mode & 0x0F) << 28);
    }

    GaussianEdit::EditMode GaussianEdit::get_edit_mode() const
    {
        return static_cast<GaussianEdit::EditMode>((edit_op >> 28) & 0x0F);
    }

    void GaussianEdit::set_edit_type(EditType type)
    {
        edit_op = (edit_op & 0xFF00FFFF) | (((int)type & 0xFF) << 16);
    }
    //get edit type
    GaussianEdit::EditType GaussianEdit::get_edit_type() const
    {
        return static_cast<GaussianEdit::EditType>((edit_op >> 16) & 0xFF);
    }

    void GaussianEdit::update_brush_buffer()
    {
        brush_tool.update_brush_buffer();
    }

    void GaussianEdit::create_brush_buffer(u32 width,u32 height)
    {
        brush_tool.create_brush_buffer(width,height);
    }

    void GaussianEdit::reset_brush()
    {
        brush_tool.reset_brush();
    }

    auto GaussianEdit::add_delete_op() -> void
    {
        if(!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<DeleteSplatEditOp>(splat));
    }
    auto GaussianEdit::add_reset_op() -> void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<ResetSplatEditOp>(splat));
    }

    auto GaussianEdit::add_selection_op(EditSelectOpType type)->void
    {
        if (!(splat && splat->ModelRef)) return;
        splat->ModelRef->download_state_buffer();
        auto filter = [this](int i)->bool{
            return splat->ModelRef->flags()[i] == 1;
        };
        UndoRedoSystem::get().add(std::make_shared<SplatSelectionOp>(splat,type,filter));
    }
    auto GaussianEdit::add_paint_op()->void
    {
		if (!(splat && splat->ModelRef)) return;
		splat->ModelRef->download_state_buffer();
        auto filter = [this](int i)->bool {
			return splat->ModelRef->flags()[i] == 1;
		};
        SplatPaintColorAdjustment new_state = { g_render_settings.paint_color.xyz, g_render_settings.paint_weight };
        UndoRedoSystem::get().add(std::make_shared<SplatPaintColorAdjustmentOp>(splat,filter,new_state));
	}

    auto GaussianEdit::add_color_adjustment_op(
        const SplatColorAdjustment& old_state,
        const SplatColorAdjustment& new_state) -> void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<SetSplatColorAdjustmentOp>(splat, old_state, new_state));
    }

    auto GaussianEdit::add_place_pivot_op(const maths::Transform& old_trans,
        const maths::Transform& new_trans,
        class Pivot* pivot_t) -> void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<PlacePivotOp>(splat, old_trans, new_trans, pivot_t));
    }
    auto GaussianEdit::add_select_all_op()->void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<SplatSelectAllOp>(splat));
    }

    auto GaussianEdit::add_select_inverse_op()->void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<SplatSelectInverseOp>(splat));
    }
    auto GaussianEdit::add_select_none_op()->void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<SplatSelectNoneOp>(splat));
    }
    auto GaussianEdit::add_lock_op()->void
    {
        //hiden
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<HidenSplatOp>(splat));
    }
    auto GaussianEdit::add_unlock_op()->void
    {
        //unhiden
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<UnHidenSplatOp>(splat));
    }
    auto GaussianEdit::add_seperate_selection_op() -> void
    {
        if (!(splat && splat->ModelRef)) return;
        std::vector<std::shared_ptr<SplatEditOperation>> ops = { std::make_shared<AddSplatOp>(splat,scene), std::make_shared<DeleteSplatEditOp>(splat) };
        auto op = std::make_shared<MultiOp>(splat, ops);
        UndoRedoSystem::get().add(op);
    }
    auto GaussianEdit::add_duplicate_selection_op() -> void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<DuplicateSelectionSplatOp>(splat));
    }

    auto GaussianEdit::add_duplicate_selection_2_instance_op() -> void
    {
        if (!(splat && splat->ModelRef)) return;
        UndoRedoSystem::get().add(std::make_shared<AddSplatOp>(splat,scene));
    }

    auto GaussianEdit::clear_op()->void
    {
        edit_op = (edit_op & 0xFFFF0000) | 0x0000FFFF;
    }

    auto GaussianEdit::clear_op_history()->void
    {
        edit_op = 0x0000FFFF;
        UndoRedoSystem::get().clear();
    }

    void GaussianEdit::set_edit_splat(GaussianComponent* model)
    {
        splat = model;
    }
    void GaussianEdit::set_splat_transform(maths::Transform* t)
    {
        splat_transform = t;
    }

    auto GaussianEdit::has_select_gaussians() -> bool
    {
        if (!(splat && splat->ModelRef)) return false;
        return splat->ModelRef->has_select_gaussians();
    }

    auto GaussianEdit::start_transform_op(const maths::Transform& new_trans) -> void
    {
        if (!(splat && splat->ModelRef)) return;
        if(has_select_gaussians())
        {
            palette_map.clear();
            auto& transform_index = splat->ModelRef->transform_index();
            auto& state = splat->ModelRef->state();
            for (auto i = 0; i < state.size();i++) 
            {
                if (state[i] == SELECT_STATE) {
                    auto old_idx = transform_index[i];
                    u32 new_idx = 0;
                    if (palette_map.find(old_idx) == palette_map.end()) {
                        glm::mat4 mat = splat->ModelRef->splat_transforms[old_idx];
                        new_idx = splat->ModelRef->splat_transforms.add_transform(glm::transpose(mat));
                        palette_map[old_idx] = new_idx;
                    }
                    else {
                        new_idx = palette_map[old_idx];
                    }
                    transform_index[i] = new_idx;
                }
            }
            splat->ModelRef->update_transform_index();
        }
    }

    auto GaussianEdit::update_transform_op(
        const maths::Transform& old_transform,
        const maths::Transform& new_transform) -> void
    {
        if (!(splat && splat->ModelRef)) return;
        auto delta_matrix = new_transform.get_world_matrix() * glm::inverse(old_transform.get_world_matrix());
        if (has_select_gaussians())
        {
            auto transform_mat = splat_transform->get_world_matrix();
            for (auto [old_idx, new_idx] : palette_map)
            {
                glm::mat4 mat = splat->ModelRef->splat_transforms[old_idx];
                auto mat2 = glm::inverse(transform_mat) * delta_matrix * transform_mat * glm::transpose(mat);
                splat->ModelRef->splat_transforms.set_transform(new_idx, mat2);
            }
        }
        splat->ModelRef->make_world_bound_dirty();
    }

    auto GaussianEdit::end_transform_op(
        const maths::Transform& old_trans,
        const maths::Transform& new_trans,
        class Pivot* pivot_t) -> void
    {
        if (!(splat && splat->ModelRef)) return;
        if(has_select_gaussians()){
            auto transform_matrix = splat_transform->get_world_matrix();
            auto top = std::make_shared<SplatTransformOp>(splat,old_trans, new_trans, transform_matrix, palette_map);
            auto pop = std::make_shared<PlacePivotOp>(splat, old_trans,new_trans, pivot_t);
            std::vector<std::shared_ptr<SplatEditOperation>> ops = { top,pop };
            auto op = std::make_shared<MultiOp>(splat, ops);
            UndoRedoSystem::get().add(op);
        }
        else
        {
            auto old_m = old_trans.get_world_matrix();
            auto new_m = new_trans.get_world_matrix();
            if (old_m != new_m) 
            {
                auto delta_matrix = new_trans.get_world_matrix() * glm::inverse(old_trans.get_world_matrix());
                auto old_t = glm::inverse(delta_matrix) * (splat_transform->get_world_matrix());
                auto top = std::make_shared<SplatEntityTransformOp>(splat, old_t, *splat_transform, splat_transform);
                auto pop = std::make_shared<PlacePivotOp>(splat, old_trans, new_trans, pivot_t);
                std::vector<std::shared_ptr<SplatEditOperation>> ops = { top,pop };
                auto op = std::make_shared<MultiOp>(splat, ops);
                UndoRedoSystem::get().add(op);
            }
        }
    }

    extern 	auto gpu_sort(rg::RenderGraph& rg,
        rg::Handle<rhi::GpuBuffer>& keys_src,
        rg::Handle<rhi::GpuBuffer>& values_src,
        u32 count) -> std::pair<rg::Handle<rhi::GpuBuffer>, rg::Handle<rhi::GpuBuffer>>;

    
    auto GaussianEdit::reset_splat_intersected()->void
    {
        splat_intersected = false;
    }

    auto GaussianEdit::get_splat_intersected()->bool
    {
        if (splat_intersected)
            return true;
        return false;
    }

    auto GaussianEdit::intersect_splat(EditSelectOpType op)->bool
    {
        if(!(splat && splat->ModelRef)) return false;
        const auto edit_mode = get_edit_mode();

        auto splat_model = splat->ModelRef.get();
        auto renderer = Application::get().get_renderer();
      
        rhi::RenderPassDesc pickdesc = {
            {
                rhi::RenderPassAttachmentDesc::create(PixelFormat::R32_UInt).clear_input(),
            },
            rhi::RenderPassAttachmentDesc::create(PixelFormat::D32_Float).load_input()
        };
        static auto gs_pick_render_pass = g_device->create_render_pass(pickdesc);
        auto buffer_id = renderer->get_buf_id(splat_model);
        auto bindless_set = renderer->binldess_descriptorset();
        rg::RenderGraph rg;
        renderer->register_event_render_graph(rg);
        {
            const auto& color_img = *renderer->get_main_render_image();
            const auto edit_box = bouding_box().transformed(edit_transform.get_local_matrix());
            const auto center = edit_transform.get_local_position() + bdsphere.get_center();
            const auto scale = glm::compMax(edit_transform.get_local_scale());
            const auto radius = bdsphere.get_radius() * scale;
            const auto edit_sphere = maths::BoundingSphere(center, radius);
            const auto width = color_img.desc.extent[0];
            const auto height = color_img.desc.extent[1];
            const auto num_gaussians = splat->ModelRef->get_num_gaussians();
            const u32  max_gaussians = 20000000;
            auto pick_tex = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::R32_UInt, color_img.desc.extent_2d()), "pick_tex");
            auto depth_img = rg.create<rhi::GpuTexture>(rhi::GpuTextureDesc::new_2d(PixelFormat::D32_Float, color_img.desc.extent_2d()), "pick_depth_tex");
            rg::clear_color(rg, pick_tex, { 0.0f,0.0f,0.0f,0.0f });
            if (edit_mode == EditMode::Rings)
            {
                struct GaussianConstants {
                    glm::mat4 transform;
                    u32 mesh_index;
                    u32 surface_width;
                    u32 surface_height;
                    u32 num_gaussians;
                    glm::vec4 locked_color;
                    glm::vec4 select_color;
                    glm::vec4 tintColor;//w: transparency
                    glm::vec4 color_offset;
                }gs_constants;
                auto offset = -splat->black_point + splat->brightness;
                auto scale = 1.0f / (splat->white_point - splat->black_point);
                gs_constants.transform = glm::transpose(splat_transform->get_world_matrix());
                gs_constants.mesh_index = buffer_id;
                gs_constants.surface_width = color_img.desc.extent[0];
                gs_constants.surface_height = color_img.desc.extent[1];
                gs_constants.tintColor = glm::vec4(splat->albedo_color.xyz * scale, splat->transparency);
                gs_constants.locked_color = g_render_settings.locked_color;
                gs_constants.select_color = g_render_settings.select_color;
                gs_constants.num_gaussians = num_gaussians;
                gs_constants.color_offset = glm::vec4(glm::vec3(offset, offset, offset), splat->ModelRef->splat_size);
                auto point_list_key_buffer = rg.import_res(splat->ModelRef->points_key_buf, rhi::AccessType::Nothing);
                auto point_list_value_buffer = rg.import_res(splat->ModelRef->points_value_buf, rhi::AccessType::Nothing);
                rg::RenderPass::new_compute(
                    rg.add_pass("clear_points"), "/shaders/gaussian/clear_points.hlsl")
                    .write(point_list_key_buffer)
                    .write(point_list_value_buffer)
                    .constants(num_gaussians)
                    .dispatch({ (u32)num_gaussians,1,1 });

                rg::RenderPass::new_compute(
                    rg.add_pass("gsplat_viewz"), "/shaders/gaussian/gsplat_viewz_cs.hlsl")
                    .write(point_list_key_buffer)
                    .write(point_list_value_buffer)
                    .constants(gs_constants)
                    .raw_descriptor_set(1, bindless_set)
                    .dispatch({ (u32)num_gaussians,1,1 });
                auto [sorted_key, sorted_value] = gpu_sort(rg, point_list_key_buffer, point_list_value_buffer, num_gaussians);

                auto pickpass = rg.add_pass("pick pass");
                std::vector<std::pair<std::string, std::string>> defines;
                defines.push_back({ "GSPLAT_AA", std::to_string((u32)splat->ModelRef->antialiased()) });
                defines.push_back({ "PICK_PASS", "1" });
#if SPLAT_EDIT
                defines.push_back({ "SPLAT_EDIT", "1" });
#endif
                auto pipeline_desc = rhi::RasterPipelineDesc()
                    .with_render_pass(gs_pick_render_pass)
                    .with_cull_mode(rhi::CullMode::NONE)
                    .with_vetex_attribute(false)
                    .with_depth_test(false)
                    .with_primitive_type(rhi::PrimitiveTopType::TriangleStrip);
                auto pipeline = pickpass.register_raster_pipeline(
                    {
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Vertex).with_shader_source({"/shaders/gaussian/gsplat_pick_vs.hlsl","main",defines}),
                        rhi::PipelineShaderDesc().with_stage(rhi::ShaderPipelineStage::Pixel).with_shader_source({"/shaders/gaussian/gsplat_pick_ps.hlsl","main",defines})
                    },
                    std::move(pipeline_desc)
                );
                pickpass.render([this,
                    color_ref = pickpass.raster(pick_tex, rhi::AccessType::ColorAttachmentWrite),
                    depth_ref = pickpass.raster(depth_img, rhi::AccessType::DepthAttachmentWriteStencilReadOnly),
                    num_gaussians, gs_constants, bindless_set,
                    sorted_buffer_ref = pickpass.read(sorted_value, rhi::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer),
                    pipeline_raster = std::move(pipeline)](rg::RenderPassApi& api) mutable {
                        auto [width, height, _] = color_ref.desc.extent;

                        auto dynamic_offset = api.dynamic_constants()
                            ->push(gs_constants);

                        api.begin_render_pass(
                            *gs_pick_render_pass,
                            { width, height },
                        {
                            std::pair{color_ref,rhi::GpuTextureViewDesc()},
                        },
                        std::pair{ depth_ref, rhi::GpuTextureViewDesc().with_aspect_mask(rhi::ImageAspectFlags::DEPTH) }
                        );

                        api.set_default_view_and_scissor({ width,height });
                        //write sorted_value buffer
                        std::vector<rg::RenderPassBinding> bindings = {
                            rg::RenderPassBinding::DynamicConstants(dynamic_offset),
                            sorted_buffer_ref.bind()
                        };

                        auto res = rg::RenderPassPipelineBinding<rg::RgRasterPipelineHandle>::from(pipeline_raster)
                            .descriptor_set(0, &bindings)
                            .raw_descriptor_set(1, bindless_set);
                        auto bound_raster = api.bind_raster_pipeline(res);

                        auto device = api.device();

                        device->draw_instanced(api.cb, 4, num_gaussians, 0, 0);

                        api.end_render_pass();
                    });

                pickpass.rg->record_pass(std::move(pickpass.pass));
            }
            struct GaussianEditConstants {
                u32 surface_width;
                u32 surface_height;
                u32 num_gaussians;
                f32 point_size;
                
                glm::mat4 transform;

                glm::vec3 edit_box_min;
                uint edit_thick_ness;
                glm::vec3 edit_box_max;
                uint edit_value; 

                float edit_sphere_radius;
                glm::vec3 edit_sphere_center;
                
                glm::vec4 edit_rect;

                glm::vec2 mouse_pos;
                float gs_splat_size;
                uint buf_id;
            }edit_constants;

            edit_constants = { width, height, (u32)num_gaussians, glm::clamp(g_render_settings.gs_point_size,1.0f,100.0f)};
            edit_constants.transform = glm::transpose(splat_transform->get_world_matrix());
            edit_constants.edit_box_min = edit_box.min();
            edit_constants.edit_box_max = edit_box.max();
            edit_constants.edit_rect = glm::vec4(rect_area().get_position(), rect_area().get_position() + rect_area().get_size());
            edit_constants.edit_value = edit_op;
            edit_constants.edit_thick_ness = ((brush_points().size() << 16) & 0xFFFF0000) | (brush_thick_ness() & 0x0000FFFF);
            edit_constants.edit_sphere_center = edit_sphere.get_center();
            edit_constants.edit_sphere_radius = edit_sphere.get_radius();
            edit_constants.mouse_pos = mouse_pos;
            edit_constants.gs_splat_size = splat->ModelRef->splat_size;
            edit_constants.buf_id = buffer_id;
            auto bursh_buffer = rg.import_res(brush_tool.brush_buffer(), rhi::AccessType::Nothing);
            std::vector<std::pair<std::string, std::string>> defines;
            if (edit_mode == EditMode::Points) // Centers
                defines = std::vector<std::pair<std::string, std::string>>{
                    {"MODE_CENTERS", "1"}
                };
            else
                defines = std::vector<std::pair<std::string, std::string>>{
                    {"MODE_RINGS", "1"}
            };
#if SPLAT_EDIT
            defines.push_back({ "SPLAT_EDIT", "1" });
#endif
            rg::RenderPass::new_compute(
                rg.add_pass("gs_edit"), "/shaders/gaussian/gsplat_intersect.hlsl", defines)
                .read(bursh_buffer)
                .read(pick_tex)
                .constants(edit_constants)
                .raw_descriptor_set(1, bindless_set)
                .dispatch({ (u32)num_gaussians,1,1 });

            rg.execute();
        }
        splat_intersected = true;
        return true;
    }
}
