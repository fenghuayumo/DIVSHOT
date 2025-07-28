#pragma once
#include "core/base_type.h"
#include <glm/glm.hpp>
namespace diverse
{
    enum class RenderMode
    {
        Hybrid = 0,
        PT = 1,
        Unlit = 2,
        WireFrame = 3,
    };

    enum class DebugShadingMode
    {
        SHADING_MODE_DEFAULT = 0,
        SHADING_MODE_NO_TEXTURES,
        SHADING_MODE_DIFFUSE_GI,
        SHADING_MODE_REFLECTIONS,
        SHADING_MODE_RTX_OFF,
        SHADING_MODE_IRCACHE,
        Max
    };

    struct RenderSettings
    {
        i32  gs_vis_type;
        f32  gs_point_size = 2.0f;
        u8   splat_edit_render_mode = 0xff;//0: Centers, 1: Rings
        glm::vec4  select_color = glm::vec4(1,1,0,1);
        glm::vec4  locked_color = glm::vec4(0,0,0,0.045);
        glm::vec3  paint_color = glm::vec3(1);
        f32        paint_weight = 0.5f;
        RenderMode render_mode = RenderMode::PT;
        DebugShadingMode shade_mode = DebugShadingMode::SHADING_MODE_DEFAULT;

        bool                show_wireframe = false;
        bool                use_dynamic_adaptation = false;
        f32                 ev_shift = 0.0f;
        f32                 dynamic_adaptation_speed = 0.0f;
        //"Luminance histogram low clip
        f32                 dynamic_adaptation_low_clip = 0.0f;
        //"Luminance histogram high clip
        f32                 dynamic_adaptation_high_clip = 0.0f;
        
        f32                 contrast = 1.0f;

    };

    extern RenderSettings g_render_settings;
}