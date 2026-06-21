#include "editor_systems_plugin.h"
#include "editor.h"
#include "scene/scene.h"
#include "scene/entity_manager.h"
#include "scene/components/global_transform_component.h"
#include "scene/components/transform_component.h"
#include "scene/camera/camera.h"
#include "scene/camera/editor_camera.h"
#include "scene/component/environment.h"
#include "engine/application.h"
#include "engine/input.h"
#include "engine/engine.h"
#include "renderer/defered_renderer.h"
#include "imgui/Plugins/ImGuizmo.h"

#ifdef Environment
#undef Environment
#endif

namespace diverse::schedule
{
    namespace
    {
        void mark_camera_transform_dirty(Editor* editor, Scene* scene)
        {
            if (!editor || !scene)
                return;

            const entt::entity camera_entity = editor->get_editor_camera_entity();
            if (camera_entity == entt::null)
                return;

            auto& registry = scene->get_registry();
            if (!registry.valid(camera_entity))
                return;

            if (auto* global_transform = registry.try_get<GlobalTransform>(camera_entity))
                global_transform->dirty = true;
        }

        void ensure_default_environment(Scene* scene)
        {
            if (!scene)
                return;

            auto environments = scene->get_entity_manager()->get_entities_with_type<Environment>();
            if (!environments.empty())
                return;

            auto environment = scene->get_entity_manager()->create("Environment");
            environment.add_component<Environment>();
            environment.get_component<Environment>().load(Application::get().get_renderer()->get_device());
        }
    }

    EditorSystemsPlugin::EditorSystemsPlugin(Editor* editor)
        : m_editor(editor)
    {
    }

    void EditorSystemsPlugin::build(Schedule& schedule)
    {
        Editor* editor = m_editor;

        schedule.add_system("EditorEnsureEnvironment", [&schedule]() {
            ensure_default_environment(schedule.scene());
        })
        .in_stage(SystemStage::Startup)
        .add_label("editor")
        .set_thread_local_flag();

        schedule.add_system("EditorCameraControl", [editor, &schedule]() {
            if (!editor || !editor->get_scene_view_active())
                return;
            if (!Application::get().get_scene_active())
                return;
            if (editor->is_camera_transitioning())
                return;

            auto* scene = schedule.scene();
            auto* controller = editor->get_editor_camera_controller();
            auto* transform = editor->get_editor_camera_transform();
            auto* camera = editor->get_camera();
            if (!scene || !controller || !transform || !camera)
                return;

            const float dt = static_cast<float>(Engine::get_time_step().get_seconds());
            const glm::vec2 mouse_pos = Input::get().get_mouse_position();
            controller->set_camera(camera);
            controller->handle_mouse(*transform, dt, mouse_pos.x, mouse_pos.y);
            controller->handle_keyboard(*transform, dt);
            mark_camera_transform_dirty(editor, scene);
        })
        .in_stage(SystemStage::PreUpdate)
        .before("SceneGraphUpdate")
        .add_label("editor")
        .add_label("camera")
        .run_if([](WorldContext* ctx) {
            if (auto* editor = Editor::get_editor())
                return editor->should_run_scene_update();
            return true;
        })
        .set_thread_local_flag();

        schedule.add_system("EditorCameraTransition", [editor, &schedule]() {
            if (!editor || !editor->is_camera_transitioning())
                return;

            auto* scene = schedule.scene();
            auto* controller = editor->get_editor_camera_controller();
            auto* transform = editor->get_editor_camera_transform();
            if (!scene || !controller || !transform)
                return;

            constexpr float k_completion_tolerance = 0.01f;
            constexpr float k_speed_base_factor = 5.0f;

            const float dt = static_cast<float>(Engine::get_time_step().get_seconds());
            const auto camera_current_position = transform->get_local_position();

            controller->update_focal_point(*transform, glm::mix(
                camera_current_position,
                editor->get_camera_destination(),
                glm::clamp(editor->get_camera_transition_speed() * k_speed_base_factor * dt, 0.0f, 1.0f)));

            const float distance_to_destination = glm::distance(camera_current_position, editor->get_camera_destination());
            editor->set_camera_transitioning(distance_to_destination > k_completion_tolerance);
            mark_camera_transform_dirty(editor, scene);
        })
        .in_stage(SystemStage::PreUpdate)
        .after("EditorCameraControl")
        .before("SceneGraphUpdate")
        .add_label("editor")
        .add_label("camera")
        .run_if([](WorldContext* ctx) {
            if (auto* editor = Editor::get_editor())
                return editor->should_run_scene_update();
            return true;
        })
        .set_thread_local_flag();

        schedule.add_system("EditorSceneViewShortcuts", [editor]() {
            if (!editor || !editor->get_scene_view_active())
                return;

            auto* scene = editor->get_current_scene();
            if (!scene)
                return;

            auto& registry = scene->get_registry();

            if (!editor->get_selected().empty() && Input::get().get_key_pressed(InputCode::Key::F))
            {
                const entt::entity selected = editor->get_selected().front();
                if (registry.valid(selected))
                {
                    if (auto* global = registry.try_get<GlobalTransform>(selected))
                        editor->focus_camera(global->position(), 2.0f, 2.0f);
                }
            }

            if (Input::get().get_key_held(InputCode::Key::O))
                editor->focus_camera(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f, 2.0f);

            if (Input::get().get_key_pressed(InputCode::Key::H))
                editor->toggle_helper_panel();

            if (Input::get().get_key_pressed(InputCode::Key::Space))
                editor->toggle_render_mode();
        })
        .in_stage(SystemStage::Update)
        .after("EditorCameraTransition")
        .add_label("editor")
        .run_if([](WorldContext* ctx) {
            if (auto* editor = Editor::get_editor())
                return editor->should_run_scene_update();
            return true;
        })
        .set_thread_local_flag();

        schedule.add_system("EditorGizmoShortcuts", [editor]() {
            if (!editor || !editor->get_scene_view_active())
                return;

            if (Input::get().get_mouse_held(InputCode::MouseKey::ButtonRight) || ImGuizmo::IsUsing())
                return;

            if (Input::get().get_key_pressed(InputCode::Key::T))
                editor->set_imguizmo_operation(ImGuizmo::OPERATION::TRANSLATE);
            if (Input::get().get_key_pressed(InputCode::Key::R))
                editor->set_imguizmo_operation(ImGuizmo::OPERATION::ROTATE);
            if (Input::get().get_key_pressed(InputCode::Key::Y))
                editor->set_imguizmo_operation(ImGuizmo::OPERATION::SCALE);
            if (Input::get().get_key_pressed(InputCode::Key::U))
                editor->set_imguizmo_operation(ImGuizmo::OPERATION::UNIVERSAL);
        })
        .in_stage(SystemStage::Update)
        .after("EditorSceneViewShortcuts")
        .add_label("editor")
        .run_if([](WorldContext* ctx) {
            if (auto* editor = Editor::get_editor())
                return editor->should_run_scene_update();
            return true;
        })
        .set_thread_local_flag();
    }

}
