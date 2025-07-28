#pragma once
#include "view_constants.h"

#define IRACHE_CASCADE_COUNT 12
namespace diverse
{
    enum class RenderOverrideFlags
    {
        FORCE_FACE_NORMALS = 1 << 0,
        NO_NORMAL_MAPS = 1 << 1,
        FLIP_NORMAL_MAP_YZ = 1 << 2,
        NO_METAL = 1 << 3
    };
    struct RenderOverride
    {
        u32 flags = 0;
        f32 material_roughness_scale = 1.0f;
        glm::ivec2  padding;
    };

    struct SurfelCascadeConstants
    {
        glm::ivec4    origin;
        glm::ivec4    voxels_scrolled_this_frame;
    };

    struct FrameConstants
    {
        ViewConstants   view_constants;

        glm::vec4    sun_dir = glm::vec4(0, 1, 0, 1);

        u32         frame_index = 0;
        f32         delta_time_seconds = 0;
        f32         sun_angular_radius_cos = 0;
        u32         triangle_light_count = 0;

        glm::vec4    sun_color_multiplier = glm::vec4(1, 1, 1, 1);
        glm::vec4    sky_ambient;

        f32 pre_exposure = 1.0f;
        f32 pre_exposure_prev = 1.0f;
        f32 pre_exposure_delta = 0.0f;
        u32 scene_lights_count;

        RenderOverride render_overrides;

        glm::vec4 ircache_grid_center;
        std::array<SurfelCascadeConstants, IRACHE_CASCADE_COUNT>  ircache_cascades;
        //SurfelCascadeConstants ircache_cascades[IRACHE_CASCADE_COUNT];
    };
}