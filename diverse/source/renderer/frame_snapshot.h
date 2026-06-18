#pragma once

#include "core/base_type.h"
#include "light.h"

#include <glm/vec3.hpp>

namespace diverse
{
    struct GridFrameParams
    {
        bool valid = false;
        f32 near_plane = 0.0f;
        f32 far_plane = 0.0f;
        glm::vec3 camera_position = glm::vec3(0.0f);
        glm::vec3 camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
    };

    struct FrameLight
    {
        u64 stable_id = 0;
        LightShaderData shader_data = {};
    };

    struct GpuSceneDirtyState
    {
        bool gaussian_resources = false;
        bool point_resources = false;
        bool mesh_resources = false;
        bool material_resources = false;
        bool bindless_resources = false;
        bool instance_data = false;
        bool acceleration_structures = false;

        auto any() const -> bool
        {
            return gaussian_resources ||
                   point_resources ||
                   mesh_resources ||
                   material_resources ||
                   bindless_resources ||
                   instance_data ||
                   acceleration_structures;
        }
    };
}
