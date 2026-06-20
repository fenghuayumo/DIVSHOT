#pragma once

#include <glm/gtc/quaternion.hpp>
#include <string>

namespace diverse
{
    class Scene;

    struct SceneImportMeta
    {
        bool has_camera = false;
        glm::vec3 camera_position{0.0f};
        glm::quat camera_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        float camera_fov_deg = 60.0f;
        float camera_near = 0.1f;
        float camera_far = 1000.0f;
    };

    class JsonSceneLoader
    {
    public:
        static bool is_json_scene_file(const std::string& file_path);
        static bool load(Scene& scene, const std::string& scene_file_path, SceneImportMeta& out_meta);
    };
}
