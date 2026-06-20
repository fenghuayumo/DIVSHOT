#include "precompile.h"
#include "json_scene_loader.h"

#include "scene.h"
#include "scene/component/mesh_model_component.h"
#include "scene/component/environment.h"
#include "scene/entity_manager.h"
#include "scene/scene_graph.h"
#include "scene/components/global_transform_component.h"
#include "scene/components/light_component.h"
#include "scene/components/transform_component.h"
#include "assets/model_asset.h"
#include "maths/transform.h"
#include "utility/string_utils.h"
#include "core/ds_log.h"

#include <json/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <unordered_map>

namespace diverse
{
    namespace
    {
        using json = nlohmann::json;

        std::filesystem::path normalize_path(const std::filesystem::path& path)
        {
            return path.lexically_normal();
        }

        std::string to_forward_slashes(const std::filesystem::path& path)
        {
            std::string result = path.string();
            std::replace(result.begin(), result.end(), '\\', '/');
            return result;
        }

        glm::vec3 read_vec3(const json& node, const glm::vec3& default_value = glm::vec3(0.0f))
        {
            if (!node.is_array() || node.size() < 3)
                return default_value;
            return glm::vec3(node[0].get<float>(), node[1].get<float>(), node[2].get<float>());
        }

        glm::vec3 read_scale(const json& node)
        {
            if (node.is_number())
            {
                const float uniform = node.get<float>();
                return glm::vec3(uniform);
            }
            return read_vec3(node, glm::vec3(1.0f));
        }

        glm::quat read_rotation_quat(const json& node)
        {
            if (!node.is_array() || node.size() < 4)
                return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            const float x = node[0].get<float>();
            const float y = node[1].get<float>();
            const float z = node[2].get<float>();
            const float w = node[3].get<float>();
            return glm::normalize(glm::quat(w, x, y, z));
        }

        glm::quat read_camera_rotation_quat(const json& node)
        {
            // External .scene.json cameras use -Z as forward. The editor camera
            // controls and renderer treat +Z as forward, so preserve camera up
            // while flipping the local forward axis.
            static const glm::quat z_forward_conversion =
                glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
            return glm::normalize(read_rotation_quat(node) * z_forward_conversion);
        }

        void apply_transform(maths::Transform& transform, const json& node)
        {
            if (node.contains("translation"))
                transform.set_local_position(read_vec3(node["translation"]));

            if (node.contains("rotation"))
                transform.set_local_orientation(read_rotation_quat(node["rotation"]));
            else if (node.contains("euler"))
                transform.set_local_orientation(read_vec3(node["euler"]));

            if (node.contains("scaling"))
                transform.set_local_scale(read_scale(node["scaling"]));

            // Don't set world matrix here - let scene graph update handle it
            transform.update_matrices();
        }

        void apply_transform(::diverse::Transform& transform, const json& node)
        {
            if (node.contains("translation"))
                transform.local_position = read_vec3(node["translation"]);

            if (node.contains("rotation"))
                transform.local_rotation = read_rotation_quat(node["rotation"]);
            else if (node.contains("euler"))
                transform.local_rotation = glm::quat(read_vec3(node["euler"]));

            if (node.contains("scaling"))
                transform.local_scale = read_scale(node["scaling"]);
        }

        void apply_entity_transform(Entity entity, const json& node, Entity parent = {})
        {
            auto& transform = entity.get_or_add_component<::diverse::Transform>();
            apply_transform(transform, node);
            entity.get_or_add_component<GlobalTransform>().dirty = true;
        }

        std::filesystem::path resolve_scene_media_path(
            const std::filesystem::path& scene_directory,
            const std::string& relative_path)
        {
            if (relative_path.empty())
                return {};

            std::filesystem::path candidate(relative_path);
            if (candidate.is_absolute() && std::filesystem::exists(candidate))
                return normalize_path(candidate);

            return normalize_path(scene_directory / candidate);
        }

        std::filesystem::path resolve_environment_map_path(
            const std::filesystem::path& scene_directory,
            const std::string& relative_path)
        {
            auto resolved = resolve_scene_media_path(scene_directory, relative_path);
            if (std::filesystem::exists(resolved))
                return resolved;

            if (resolved.extension() == ".dds")
            {
                auto hdr_candidate = resolved;
                hdr_candidate.replace_extension(".hdr");
                if (std::filesystem::exists(hdr_candidate))
                {
                    DS_LOG_WARN("Environment map '{}' not found, using HDR fallback '{}'",
                        resolved.string(), hdr_candidate.string());
                    return hdr_candidate;
                }
            }

            const auto env_folder = normalize_path(scene_directory / "EnvironmentMaps");
            if (std::filesystem::exists(env_folder))
            {
                for (const auto& entry : std::filesystem::directory_iterator(env_folder))
                {
                    if (!entry.is_regular_file())
                        continue;

                    const auto ext = stringutility::to_lower(entry.path().extension().string());
                    if (ext == ".hdr" || ext == ".exr")
                    {
                        DS_LOG_WARN("Environment map '{}' not found, using fallback '{}'",
                            resolved.string(), entry.path().string());
                        return normalize_path(entry.path());
                    }
                }
            }

            return resolved;
        }

        std::shared_ptr<ModelAsset> load_mesh_model_preserve_origin(
            const std::string& absolute_path,
            std::unordered_map<std::string, std::shared_ptr<ModelAsset>>& model_cache)
        {
            const auto found = model_cache.find(absolute_path);
            if (found != model_cache.end())
                return found->second;

            auto model = std::make_shared<ModelAsset>();
            model->load_model(absolute_path, true);
            model_cache.emplace(absolute_path, model);
            return model;
        }

        void setup_environment(Scene& scene, const std::filesystem::path& scene_directory, const json& node)
        {
            const std::string env_path = node.value("path", std::string{});
            if (env_path.empty())
                return;

            const auto resolved = resolve_environment_map_path(scene_directory, env_path);
            const auto ext = stringutility::to_lower(resolved.extension().string());

            Entity environment_entity;
            auto environment_view = scene.get_entity_manager()->get_entities_with_type<Environment>();
            if (environment_view.empty())
            {
                environment_entity = scene.get_entity_manager()->create("Environment");
                environment_entity.add_component<Environment>();
            }
            else
            {
                environment_entity = environment_view.front();
            }

            auto& environment = environment_entity.get_component<Environment>();
            if (ext == ".hdr" || ext == ".exr")
            {
                environment.mode = Environment::Mode::HDR;
                environment.load_hdr(resolved.string());
            }
            else
            {
                DS_LOG_WARN("Unsupported environment map format '{}'; using SunSky fallback", resolved.string());
                environment.mode = Environment::Mode::SunSky;
            }

            if (node.contains("radianceScale"))
            {
                const glm::vec3 scale = read_vec3(node["radianceScale"], glm::vec3(1.0f));
                environment.intensity = (scale.x + scale.y + scale.z) / 3.0f;
            }

            if (node.contains("rotation"))
            {
                if (node["rotation"].is_array() && !node["rotation"].empty())
                    environment.theta = node["rotation"][0].get<float>() * (180.0f / glm::pi<float>());
                else if (node["rotation"].is_number())
                    environment.theta = node["rotation"].get<float>() * (180.0f / glm::pi<float>());
            }

            environment.dirty_flag = true;
        }

        void apply_camera_node(const json& node, SceneImportMeta& out_meta)
        {
            out_meta.has_camera = true;
            out_meta.camera_position = read_vec3(node["translation"], out_meta.camera_position);
            out_meta.camera_rotation = read_camera_rotation_quat(node["rotation"]);
            if (node.contains("verticalFov"))
                out_meta.camera_fov_deg = glm::degrees(node["verticalFov"].get<float>());
            if (node.contains("zNear"))
                out_meta.camera_near = node["zNear"].get<float>();
            if (node.contains("zFar"))
                out_meta.camera_far = node["zFar"].get<float>();
        }

        Entity create_light_entity(Scene& scene, const std::string& node_name, const std::string& light_type, const json& node, Entity parent)
        {
            Entity entity = scene.get_entity_manager()->create(node_name);
            if (parent)
                entity.set_parent(parent);

            // Add common light component
            auto& light_common = entity.add_component<LightCommon>();

            // Parse common light properties
            if (node.contains("intensity"))
                light_common.intensity = node["intensity"].get<float>();

            if (node.contains("color"))
                light_common.radiance = read_vec3(node["color"], glm::vec3(1.0f));

            if (node.contains("radiance"))
                light_common.radiance = read_vec3(node["radiance"], glm::vec3(1.0f));

            // Add type-specific component
            if (light_type == "DirectionalLight")
            {
                auto& dir_light = entity.add_component<DirectionalLight>();
                if (node.contains("angularSize"))
                    dir_light.angular_size = node["angularSize"].get<float>();
            }
            else if (light_type == "PointLight")
            {
                auto& point_light = entity.add_component<PointLight>();
                if (node.contains("radius"))
                    point_light.radius = node["radius"].get<float>();
            }
            else if (light_type == "SpotLight")
            {
                auto& spot_light = entity.add_component<SpotLight>();
                if (node.contains("radius"))
                    spot_light.radius = node["radius"].get<float>();
                if (node.contains("innerAngle"))
                    spot_light.inner_angle = node["innerAngle"].get<float>();
                if (node.contains("outerAngle"))
                    spot_light.outer_angle = node["outerAngle"].get<float>();
            }
            else if (light_type == "RectLight")
            {
                auto& rect_light = entity.add_component<RectLight>();
                if (node.contains("width"))
                    rect_light.width = node["width"].get<float>();
                if (node.contains("height"))
                    rect_light.height = node["height"].get<float>();
            }
            else if (light_type == "DiskLight")
            {
                auto& disk_light = entity.add_component<DiskLight>();
                if (node.contains("radius"))
                    disk_light.radius = node["radius"].get<float>();
            }
            else if (light_type == "CylinderLight")
            {
                auto& cylinder_light = entity.add_component<CylinderLight>();
                if (node.contains("radius"))
                    cylinder_light.radius = node["radius"].get<float>();
                if (node.contains("length"))
                    cylinder_light.length = node["length"].get<float>();
            }

            apply_entity_transform(entity, node, parent);

            return entity;
        }

        void load_graph_recursive(
            Scene& scene,
            const std::filesystem::path& scene_directory,
            const std::vector<std::string>& model_paths,
            const json& nodes,
            Entity parent,
            SceneImportMeta& out_meta,
            std::unordered_map<std::string, std::shared_ptr<ModelAsset>>& model_cache)
        {
            if (!nodes.is_array())
                return;

            for (const auto& node : nodes)
            {
                if (!node.is_object())
                    continue;

                const std::string node_name = node.value("name", std::string{"Node"});
                const std::string node_type = node.value("type", std::string{});
                const bool has_children = node.contains("children") && node["children"].is_array();

                if (node.contains("model"))
                {
                    const int model_index = node["model"].get<int>();
                    if (model_index < 0 || model_index >= static_cast<int>(model_paths.size()))
                    {
                        DS_LOG_WARN("Scene graph node '{}' references missing model {}", node_name, model_index);
                        continue;
                    }

                    const auto& model_path = model_paths[static_cast<size_t>(model_index)];
                    if (!std::filesystem::exists(model_path))
                    {
                        DS_LOG_ERROR("Model file does not exist for node '{}': {}", node_name, model_path);
                        continue;
                    }

                    Entity entity = scene.get_entity_manager()->create(node_name);
                    if (parent)
                        entity.set_parent(parent);

                    auto model = load_mesh_model_preserve_origin(model_path, model_cache);
                    entity.add_component<MeshModelComponent>(model);

                    apply_entity_transform(entity, node, parent);
                    continue;
                }

                Entity container;
                if (has_children && node_type.empty())
                {
                    container = scene.get_entity_manager()->create(node_name);
                    if (parent)
                        container.set_parent(parent);

                    apply_entity_transform(container, node, parent);
                }

                if (node_type == "EnvironmentLight")
                    setup_environment(scene, scene_directory, node);
                else if (node_type == "PerspectiveCamera" || node_type == "PerspectiveCameraEx")
                    apply_camera_node(node, out_meta);
                else if (node_type == "DirectionalLight" || node_type == "PointLight" ||
                         node_type == "SpotLight" || node_type == "RectLight" ||
                         node_type == "DiskLight" || node_type == "CylinderLight")
                {
                    create_light_entity(scene, node_name, node_type, node, parent);
                }

                if (has_children)
                {
                    const Entity child_parent = container ? container : parent;
                    load_graph_recursive(scene, scene_directory, model_paths, node["children"], child_parent, out_meta, model_cache);
                }
            }
        }
    }

    bool JsonSceneLoader::is_json_scene_file(const std::string& file_path)
    {
        const std::string lower = stringutility::to_lower(file_path);
        return lower.size() >= 11 && lower.compare(lower.size() - 11, 11, ".scene.json") == 0;
    }

    bool JsonSceneLoader::load(Scene& scene, const std::string& scene_file_path, SceneImportMeta& out_meta)
    {
        out_meta = {};

        if (!std::filesystem::exists(scene_file_path))
        {
            DS_LOG_ERROR("JSON scene file not found: {}", scene_file_path);
            return false;
        }

        std::ifstream input(scene_file_path);
        if (!input)
        {
            DS_LOG_ERROR("Failed to open JSON scene file: {}", scene_file_path);
            return false;
        }

        json document;
        try
        {
            input >> document;
        }
        catch (const std::exception& e)
        {
            DS_LOG_ERROR("Failed to parse JSON scene '{}': {}", scene_file_path, e.what());
            return false;
        }

        if (!document.is_object())
        {
            DS_LOG_ERROR("Invalid JSON scene structure in '{}'", scene_file_path);
            return false;
        }

        const auto scene_directory = normalize_path(std::filesystem::path(scene_file_path).parent_path());

        std::vector<std::string> model_paths;
        if (document.contains("models") && document["models"].is_array())
        {
            for (const auto& model : document["models"])
            {
                if (!model.is_string())
                    continue;

                const auto resolved = resolve_scene_media_path(scene_directory, model.get<std::string>());
                model_paths.push_back(to_forward_slashes(resolved));
            }
        }

        std::unordered_map<std::string, std::shared_ptr<ModelAsset>> model_cache;
        if (document.contains("graph"))
            load_graph_recursive(scene, scene_directory, model_paths, document["graph"], {}, out_meta, model_cache);

        scene.update_scene_graph();

        const auto mesh_count = scene.get_entity_manager()->get_entities_with_type<MeshModelComponent>().size();
        DS_LOG_INFO("Loaded JSON scene '{}' with {} model paths, {} mesh entities",
            scene_file_path, model_paths.size(), mesh_count);
        return true;
    }
}
