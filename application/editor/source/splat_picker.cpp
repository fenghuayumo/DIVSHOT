#include "scene_view_panel.h"
#include <scene/scene_manager.h>
#include <scene/entity_manager.h>
#include <assets/gaussian_model.h>
#include <utility/thread_pool.h>
#include <algorithm>
#include "gaussian_edit.h"
namespace diverse
{
    extern glm::vec2 cs_to_uv(glm::vec2 cs);


    auto SceneViewPanel::pick_splat(GaussianModel* splat,
                    const glm::vec2& mouse_pos,
                    const maths::Ray& ray,
                    const glm::mat4& model_transform,
                    const maths::Transform& camera_transform,
                    const glm::mat4& proj,
                    u32 surface_width,
                    u32 surface_height,
                    glm::vec3& isect_points)->bool
	{
        //auto box = splat->get_world_bounding_box(model_transform);
        auto& pos = splat->position();
        auto& splat_transform_index = splat->transform_index();
        if ( ImGui::IsWindowFocused())
        {
            auto& gs_edit = GaussianEdit::get();
            const auto edit_type = gs_edit.get_edit_type();
            const auto edit_mode = gs_edit.get_edit_mode();
            //if(edit_type != GaussianEdit::EditType::None) return false;
            gs_edit.create_brush_buffer(surface_width, surface_height);
            gs_edit.rect_area() = {mouse_pos.x, mouse_pos.y, 1.0f, 1.0f};
  
            gs_edit.set_edit_type(GaussianEdit::EditType::Rect);
            gs_edit.set_edit_mode(GaussianEdit::EditMode::Points);

            EditSelectOpType op = EditSelectOpType::Set;
            gs_edit.intersect_splat(op);
            gs_edit.reset_splat_intersected();
            //gs_edit.set_edit_type(edit_type);
            //gs_edit.set_edit_mode(edit_mode);
            splat->download_state_buffer();
            auto& select_flag = splat->flags();
            f32 closestD = 0;
            glm::vec3 closestP(0.0f);
            i32 closetSplat = -1;
            for(size_t i = 0; i < pos.size(); i++)
            {
                if( select_flag[i]  == 1)
                {
                    auto splat_pos = pos[i];
                    auto transform_index = splat_transform_index[i];
                    glm::mat4 transform = splat->splat_transforms[transform_index];
                    transform = model_transform * glm::transpose(transform);
                    glm::vec3 world_pos = transform * glm::vec4(splat_pos,1.0f);
                    Plane plane(world_pos, camera_transform.get_forward_direction());
                    if(plane.intersects_ray(ray.Origin, ray.Direction, isect_points))
                    {
                        const auto distance = glm::distance(isect_points, ray.Origin);
                        if(distance < closestD || closetSplat < 0)
                        {
                            closestD = distance;
                            closestP = world_pos;
                            closetSplat = i;
                        }
                    }
                }
            }
            isect_points = closestP;
            return closetSplat >= 0;
        }
        return false;
	}
}