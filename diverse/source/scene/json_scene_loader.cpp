#include "precompile.h"
#include "json_scene_loader.h"

#include "scene.h"
#include "scene/component/mesh_model_component.h"
#include "scene/component/environment.h"
#include "scene/entity_manager.h"
#include "scene/scene_graph.h"
#include "assets/mesh_model.h"
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

        SharedPtr<MeshModel> load_mesh_model_preserve_origin(
            const std::string& absolute_path,
            std::unordered_map<std::string, SharedPtr<MeshModel>>& model_cache)
        {
            const auto found = model_cache.find(absolute_path);
            if (found != model_cache.end())
                return found->second;

            auto model = createSharedPtr<MeshModel>();
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
            out_meta.camera_rotation = read_rotation_quat(node["rotation"]);
            if (node.contains("verticalFov"))
                out_meta.camera_fov_deg = glm::degrees(node["verticalFov"].get<float>());
            if (node.contains("zNear"))
                out_meta.camera_near = node["zNear"].get<float>();
            if (node.contains("zFar"))
                out_meta.camera_far = node["zFar"].get<float>();
        }

        void load_graph_recursive(
            Scene& scene,
            const std::filesystem::path& scene_directory,
            const std::vector<std::string>& model_paths,
            const json& nodes,
            Entity parent,
            SceneImportMeta& out_meta,
            std::unordered_map<std::string, SharedPtr<MeshModel>>& model_cache)
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

                    auto& transform = entity.get_component<maths::Transform>();
                    apply_transform(transform, node);
                    continue;
                }

                Entity container;
                if (has_children && node_type.empty())
                {
                    container = scene.get_entity_manager()->create(node_name);
                    if (parent)
                        container.set_parent(parent);

                    auto& transform = container.get_component<maths::Transform>();
                    apply_transform(transform, node);
                }

                if (node_type == "EnvironmentLight")
                    setup_environment(scene, scene_directory, node);
                else if (node_type == "PerspectiveCamera" || node_type == "PerspectiveCameraEx")
                    apply_camera_node(node, out_meta);

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

        std::unordered_map<std::string, SharedPtr<MeshModel>> model_cache;
        if (document.contains("graph"))
            load_graph_recursive(scene, scene_directory, model_paths, document["graph"], {}, out_meta, model_cache);

        scene.update_scene_graph();

        const auto mesh_count = scene.get_entity_manager()->get_entities_with_type<MeshModelComponent>().size();
        DS_LOG_INFO("Loaded JSON scene '{}' with {} model paths, {} mesh entities",
            scene_file_path, model_paths.size(), mesh_count);
        return true;
    }
}
