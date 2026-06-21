#include "core_systems_plugin.h"
#include "scene/scene.h"
#include "scene/components/transform_component.h"
#include "scene/components/global_transform_component.h"
#include "scene/components/parent_component.h"
#include "scene/components/light_component.h"
#include "scene/camera/camera.h"
#include "scene/camera/editor_camera.h"
#include "scene/component/environment.h"
#include "scene/sun_controller.h"
#include "utility/time_step.h"
#include "engine/input.h"
#include "engine/application.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace diverse::schedule
{

    void CoreSystemsPlugin::build(Schedule& schedule)
    {
        if (m_transform_enabled)
        {
            schedule.add_system("SceneGraphUpdate", [&schedule]() {
                if (auto* scene = schedule.scene())
                    scene->update_scene_graph();
            })
            .in_stage(SystemStage::PreUpdate)
            .add_label("transform")
            .add_label("hierarchy")
            .set_thread_local_flag();
        }

        if (m_camera_enabled)
        {
            // Camera controller update system
            schedule.add_system("CameraControllerSystem", [&schedule]() {
                auto* scene = schedule.scene();
                if (!scene) return;

                auto& registry = scene->get_registry();

                // Get mouse position
                const glm::vec2& mousePos = Input::get().get_mouse_position();

                // Find editor camera controller
                auto controller_view = registry.view<EditorCameraController>();
                if (controller_view.empty())
                    return;

                auto controller_entity = controller_view.front();
                auto& controller = registry.get<EditorCameraController>(controller_entity);

                // Get camera
                auto camera_view = registry.view<Camera>();
                if (camera_view.empty())
                    return;

                Camera* camera = &registry.get<Camera>(camera_view.front());

                // Get transform
                ::diverse::Transform* transform = registry.try_get<::diverse::Transform>(controller_entity);
                GlobalTransform* global_transform = registry.try_get<GlobalTransform>(controller_entity);

                if (transform && global_transform && Application::get().get_scene_active())
                {
                    controller.set_camera(camera);

                    // TODO: Re-enable controller handling after Transform migration is complete
                    // controller.handle_mouse(*transform, 0.016f, mousePos.x, mousePos.y);
                    // controller.handle_keyboard(*transform, 0.016f);
                }
            })
            .in_stage(SystemStage::Update)
            .after("SceneGraphUpdate")
            .add_label("camera")
            .set_thread_local_flag();
        }

        if (m_render_prepare_enabled)
        {
            // Render preparation system
            schedule.add_system("RenderPrepareSystem", [&schedule]() {
                auto* scene = schedule.scene();
                if (!scene) return;

                // Prepare render data from scene entities
                // This would extract renderable components and prepare draw calls
            })
            .in_stage(SystemStage::PreRender)
            .add_label("rendering");
        }
    }

    void PhysicsPlugin::build(Schedule& schedule)
    {
        // Velocity integration system
        schedule.add_system("VelocityIntegration", [&schedule]() {
            auto* scene = schedule.scene();
            if (!scene) return;

            auto& registry = scene->get_registry();

            // Find entities with velocity component (if it exists)
            // This is a placeholder for when velocity components are added
        })
        .in_stage(SystemStage::Update)
        .add_label("physics");

        // Collision detection system
        schedule.add_system("CollisionDetection", [&schedule]() {
            // Placeholder for collision detection
        })
        .in_stage(SystemStage::Update)
        .after("VelocityIntegration")
        .add_label("physics");
    }

    void RenderingPlugin::build(Schedule& schedule)
    {
        // Frustum culling system
        schedule.add_system("FrustumCulling", [&schedule]() {
            auto* scene = schedule.scene();
            if (!scene) return;

            auto& registry = scene->get_registry();

            // Get camera for frustum
            auto camera_view = registry.view<Camera>();
            if (camera_view.empty())
                return;

            // Perform frustum culling on renderable entities
            // This would mark entities as visible/hidden based on camera frustum
        })
        .in_stage(SystemStage::PreRender)
        .add_label("rendering");

        // Render data collection system
        schedule.add_system("CollectRenderData", [&schedule]() {
            auto* scene = schedule.scene();
            if (!scene) return;

            auto& registry = scene->get_registry();

            // Collect all renderable components and prepare for GPU upload
            // This includes meshes, gaussian splats, point clouds, etc.
        })
        .in_stage(SystemStage::Render)
        .after("FrustumCulling")
        .add_label("rendering")
        .set_thread_local_flag();

        // Environment update system (sun controller)
        schedule.add_system("EnvironmentUpdate", [&schedule]() {
            auto* scene = schedule.scene();
            if (!scene) return;

            auto& registry = scene->get_registry();

            auto env_view = registry.view<Environment>();
            if (env_view.empty())
                return;

            auto env_entity = env_view.front();
            auto& environment = registry.get<Environment>(env_entity);
            auto global_transform_view = registry.view<GlobalTransform>();

            if (!global_transform_view.empty())
            {
                auto camera_entity = global_transform_view.front();
                GlobalTransform* camera_global = registry.try_get<GlobalTransform>(camera_entity);
                auto& input = Input::get();

                if (input.get_mouse_held(InputCode::ButtonLeft) &&
                    environment.mode == Environment::Mode::SunSky &&
                    camera_global)
                {
                    auto delta = input.get_mouse_delta();
                    auto delta_x = (delta.x / 2048.0f) * 6.283f;
                    auto delta_y = (delta.y / 968.0f) * 3.1415f;
                    glm::quat ref_frame = camera_global->rotation();
                    // Keep only Y component
                    ref_frame = glm::quat(ref_frame.w, 0.0f, ref_frame.y, 0.0f);
                    ref_frame = glm::normalize(ref_frame);

                    auto& sun_controller = registry.get<SunController>(env_entity);
                    sun_controller.view_space_rotate(ref_frame, delta_x, delta_y);
                }
            }
        })
        .in_stage(SystemStage::Update)
        .add_label("environment")
        .set_thread_local_flag();
    }

} // namespace diverse::schedule
