#pragma once
#include <maths/transform.h>
#include <maths/bounding_box.h>
#include <maths/bounding_sphere.h>
#include <maths/rect.h>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <deque>
#include "brush_tool.h"
#include "splat_edit_history.h"

#define RESET_OP  0
#define DELETE_OP 1
#define UNDO_OP   2
#define REDO_OP   3

#define ALL_OP    1
#define INVERT_OP 2


namespace cv
{
   class Mat;
}
namespace diverse
{
    namespace rhi
    {
        struct GpuBuffer;
        struct GpuTexture;
    };

    class GaussianEdit : public ThreadSafeSingleton<GaussianEdit>
    {
    public:
        GaussianEdit();
        ~GaussianEdit();
        enum class EditType
        {
            None,
            Box,
            Sphere,
            Rect,
            Brush,
            Polygon,
            Lasso,
            Paint,
        };
        enum class EditMode
        {
            Points = 0,
            Rings
        };
        void set_mouse_pos(const glm::vec2& pos);
        glm::vec2 get_mouse_pos() const;
        void set_edit_mode(EditMode mode);
        EditMode get_edit_mode() const;
        void set_edit_type(EditType type);
        EditType get_edit_type() const;
        maths::BoundingBox& bouding_box() {return bdbox;}
        maths::BoundingSphere& bouding_sphere() { return bdsphere; }
        maths::Rect& rect_area() { return rect; }
        
        auto brush_thick_ness()->uint& {return brush_tool.brush_thickness;}
        auto brush_points()->std::vector<glm::ivec2>& { return brush_tool.brush_points(); }
        auto brush_buffer()->std::shared_ptr<rhi::GpuBuffer> { return brush_tool.brush_buffer(); }
        auto brush_texture()->std::shared_ptr<rhi::GpuTexture> {return brush_tool.brush_texture();}
        auto brush_mask()->std::shared_ptr<cv::Mat> {return brush_tool.brush_mask();}
        void update_brush_buffer();
        void create_brush_buffer(u32 width,u32 height);
        void reset_brush();
        template <typename Archive>
        void save(Archive& archive) const
        {
            auto edit_type = get_edit_type();
            archive(cereal::make_nvp("brushThickness", brush_tool.brush_thickness), cereal::make_nvp("edit_type", (int)edit_type));
        }

        template <typename Archive>
        void load(Archive& archive)
        {
            int edit_type;
            archive(cereal::make_nvp("brushThickness", brush_tool.brush_thickness), cereal::make_nvp("edit_type", edit_type));
            set_edit_type((EditType)edit_type);
        }
        auto    add_delete_op()->void;
        auto    add_reset_op()->void;
        auto    add_selection_op(EditSelectOpType type)->void;
        auto    add_paint_op()->void;
        auto    add_place_pivot_op(
                const maths::Transform& old_trans,
                const maths::Transform& new_trans,
                class Pivot* pivot_t)->void;
        auto    add_select_all_op()->void;
        auto    add_select_inverse_op()->void;
        auto    add_select_none_op()->void;
        auto    add_lock_op()->void;
        auto    add_unlock_op()->void;
        auto    add_color_adjustment_op(
                const SplatColorAdjustment& old_state,
                const SplatColorAdjustment& new_state)->void;
        auto    add_duplicate_selection_op()->void;
        auto    add_seperate_selection_op()->void;
        auto    add_duplicate_selection_2_instance_op()->void;
        auto    clear_op()->void;
        auto    clear_op_history()->void;
        auto    intersect_splat(EditSelectOpType op)->bool;
        void    set_edit_splat(GaussianComponent* splat);
        auto    set_splat_transform(maths::Transform* t)->void;
        auto    get_edit_transform()->maths::Transform& { return edit_transform;}
        void    set_edit_scene(Scene* sc) { scene = sc;};
        auto    has_select_gaussians()->bool;
        //transform handler
        auto    start_transform_op(const maths::Transform& new_trans)->void;
        auto    end_transform_op(
                const maths::Transform& old_trans, 
                const maths::Transform& new_trans, 
                class Pivot* pivot_t)->void;
        auto    update_transform_op(
                const maths::Transform& old_trans,
                const maths::Transform& new_trans)->void;

        auto   reset_splat_intersected()->void;
        auto   get_splat_intersected()->bool;
        auto   min_radius_ref()->float& { return min_radius; }
        auto   max_opacity_ref()->float& { return max_opacity; }
        uint                    edit_op = 0x0000FFFF;
        GaussianComponent*      splat = nullptr;
        bool                    closed_polygon = false;
        bool                    splat_intersected = false;
    protected:
        maths::BoundingSphere   bdsphere;
        maths::BoundingBox      bdbox;
        maths::Rect             rect;
        glm::vec2               mouse_pos;
        maths::Transform        edit_transform;
        maths::Transform*       splat_transform;
        Scene*                  scene;

        std::unordered_map<u32, u32> palette_map;
        std::vector<glm::mat3x4>    splat_transforms;

        BrushTool               brush_tool;
        float                   min_radius = 0.0f;
        float                   max_opacity = 1.0f;
    };
}
