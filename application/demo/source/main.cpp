#include "core/core.h"
#include "core/reference.h"
#include "core/ds_log.h"
#include "utility/time_step.h"
#include "engine/application.h"
#include "engine/window.h"
#include "engine/input.h"
#include "events/application_event.h"
#include "engine/engine.h"
#include "engine/entry_point.h"
#include "scene/scene.h"
#include "scene/entity.h"
#include "scene/schedule/schedule.h"
#include "scene/schedule/system_condition.h"
#include "scene/schedule/query.h"
#include "scene/schedule/res.h"
#include "scene/schedule/core_systems_plugin.h"
#include "scene/components/transform_component.h"
#include "scene/components/global_transform_component.h"
#include "scene/camera/camera.h"
#include "scene/component/environment.h"

#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <ctime>
#include <algorithm>

using namespace diverse;

// Demo components
struct Rotation
{
    float angle = 0.0f;
    glm::vec3 axis = glm::vec3(0, 1, 0);
};

struct DemoScheduleStats
{
    uint64_t startup_runs = 0;
    uint64_t scene_graph_runs = 0;
    uint64_t rotation_runs = 0;
    uint64_t debug_runs = 0;
    uint64_t physics_runs = 0;
    uint64_t dependency_violations = 0;
    float max_rotation_angle = 0.0f;
    bool demo_systems_enabled = true;
    bool schedule_setup = false;
};

// Custom plugin to verify the Plugin API
class DemoValidationPlugin : public schedule::Plugin
{
public:
    explicit DemoValidationPlugin(DemoScheduleStats* stats)
        : m_stats(stats)
    {
    }

    void build(schedule::Schedule& schedule) override
    {
        schedule.add_system("DemoStartupSystem", [this](schedule::WorldContext* ctx) {
            if (m_stats)
            {
                ++m_stats->startup_runs;
            }
            (void)ctx;
        })
        .in_stage(schedule::SystemStage::Startup)
        .add_label("demo_validation");
    }

    const char* name() const override { return "DemoValidation"; }

private:
    DemoScheduleStats* m_stats = nullptr;
};

class DemoScene : public Scene
{
public:
    explicit DemoScene() : Scene("Demo Scene") {}

    DemoScheduleStats& stats() { return m_stats; }
    const DemoScheduleStats& stats() const { return m_stats; }

    void on_init() override
    {
        // Scene ctor calls Scene::register_systems() only (not derived).
        // Register demo systems here after the object is fully constructed.
        setup_schedule();

        TimeStep startup_timestep;
        get_schedule()->execute_startup_stages(startup_timestep);

        std::cout << "Initializing Demo Scene..." << std::endl;
        Scene::on_init();

        auto& registry = get_registry();

        auto camera_entity = create_entity("MainCamera");
        auto& camera = camera_entity.add_component<Camera>();
        camera.set_fov(60.0f);
        camera.set_near(0.01f);
        camera.set_far(1000.0f);

        auto& cam_transform = camera_entity.get_component<::diverse::Transform>();
        cam_transform.local_position = glm::vec3(0.0f, 2.0f, 5.0f);
        cam_transform.local_rotation = glm::quat(glm::radians(glm::vec3(-10.0f, 0.0f, 0.0f)));
        camera_entity.get_or_add_component<GlobalTransform>().dirty = true;

        for (int i = 0; i < 5; ++i)
        {
            auto entity = create_entity("RotatingCube_" + std::to_string(i));
            auto& transform = entity.add_component<::diverse::Transform>();
            transform.local_position = glm::vec3(static_cast<float>(i) * 2.0f - 4.0f, 0.0f, 0.0f);
            registry.emplace<Rotation>(entity.get_handle(), 0.0f, glm::vec3(0, 1, 0));
        }

        std::cout << "Created " << registry.storage<entt::entity>().in_use() << " demo entities" << std::endl;
        print_schedule_info();
    }

private:
    DemoScheduleStats m_stats;
    bool m_rotation_ran_this_frame = false;

    void setup_schedule()
    {
        if (m_stats.schedule_setup)
        {
            return;
        }

        auto* sched = get_schedule();
        if (!sched)
        {
            return;
        }

        std::cout << "Setting up demo schedule..." << std::endl;

        sched->add_plugin(std::make_shared<DemoValidationPlugin>(&m_stats));

        auto core_plugin = std::make_shared<schedule::CoreSystemsPlugin>();
        core_plugin->enable_transform_system(false); // SceneGraphUpdate already handles hierarchy
        core_plugin->enable_camera_system(false);    // CameraController depends on TransformSystem
        sched->add_plugin(core_plugin);

        sched->add_plugin(std::make_shared<schedule::PhysicsPlugin>());

        sched->add_system("SceneGraphProbe", [this]() {
            m_rotation_ran_this_frame = false;
            ++m_stats.scene_graph_runs;
        })
        .in_stage(schedule::SystemStage::PreUpdate)
        .after("SceneGraphUpdate")
        .add_label("demo_validation");

        sched->add_system("RotationSystem", [this](schedule::WorldContext* ctx) {
            auto* registry = static_cast<entt::registry*>(ctx->registry());
            if (!registry)
            {
                return;
            }

            const float dt = static_cast<float>(ctx->timestep()->get_seconds());
            auto view = registry->view<Rotation>();
            for (auto entity : view)
            {
                auto& rot = view.get<Rotation>(entity);
                rot.angle += 1.0f * dt;
                m_stats.max_rotation_angle = std::max(m_stats.max_rotation_angle, rot.angle);
            }

            m_rotation_ran_this_frame = true;
            ++m_stats.rotation_runs;
        })
        .in_stage(schedule::SystemStage::Update)
        .after("SceneGraphUpdate")
        .add_label("demo")
        .run_if([](schedule::WorldContext* ctx) {
            auto* registry = static_cast<entt::registry*>(ctx->registry());
            return registry && registry->view<Rotation>().size() > 0;
        });

        sched->add_system("DebugInfoSystem", [this]() {
            if (!m_rotation_ran_this_frame)
            {
                ++m_stats.dependency_violations;
            }
            ++m_stats.debug_runs;
        })
        .in_stage(schedule::SystemStage::PostUpdate)
        .after("RotationSystem")
        .add_label("demo");

        sched->add_system("PhysicsProbe", [this]() {
            ++m_stats.physics_runs;
        })
        .in_stage(schedule::SystemStage::Update)
        .after("VelocityIntegration")
        .add_label("demo_validation");

        build_schedule();
        m_stats.schedule_setup = true;
    }

    void print_schedule_info() const
    {
        const auto* sched = get_schedule();
        if (!sched)
        {
            return;
        }

        std::cout << "\n=== Schedule Information ===" << std::endl;
        std::cout << "Total systems: " << sched->system_count() << std::endl;
        std::cout << "Schedule built: " << (sched->is_built() ? "Yes" : "No") << std::endl;
        std::cout << "Startup runs: " << m_stats.startup_runs << std::endl;

        const auto& order = sched->get_execution_order();
        std::cout << "\nExecution Order:" << std::endl;
        for (size_t sys_id : order)
        {
            const auto* sys = sched->get_system_by_id(sys_id);
            if (sys)
            {
                std::cout << "  [" << schedule::stage_to_string(sys->stage) << "] "
                          << sys->name << std::endl;
            }
        }
    }
};

class DemoApplication : public Application
{
public:
    DemoApplication() : Application() {}

    void init() override
    {
        Application::init();

        set_editor_state(EditorState::Play);
        get_window()->set_window_title("Schedule Demo");
        get_window()->set_event_callback(BIND_EVENT_FN(DemoApplication::handle_event));

        auto* scene_mgr = get_scene_manager();
        if (!scene_mgr)
        {
            return;
        }

        // SceneManager takes ownership; do not wrap in a local SharedPtr.
        scene_mgr->enqueue_scene(new DemoScene());
        scene_mgr->switch_scene(static_cast<int>(scene_mgr->scene_count() - 1));
        scene_mgr->apply_scene_switch();

        m_demo_scene_ptr = static_cast<DemoScene*>(scene_mgr->get_current_scene());

        std::cout << "\n=== Schedule Demo Running ===" << std::endl;
        std::cout << "Close window or press ESC to exit." << std::endl;
    }

    void update(const TimeStep& dt) override
    {
        Application::update(dt);

        if (Input::get().get_key_pressed(diverse::InputCode::Key::Escape))
        {
            set_app_state(AppState::Closing);
        }
    }

    void handle_event(Event& e) override
    {
        // Editor sets scene_save_on_close on close, which keeps the main loop alive
        // waiting for a save dialog. Demo exits immediately instead.
        if (e.GetEventType() == EventType::WindowClose)
        {
            set_app_state(AppState::Closing);
            return;
        }

        Application::handle_event(e);
    }

    void imgui_render() override
    {
        Application::imgui_render();

        ImGui::Begin("Schedule Demo Info");

        ImGui::Text("Scene: %s", m_demo_scene_ptr ? m_demo_scene_ptr->get_scene_name().c_str() : "None");

        if (m_demo_scene_ptr)
        {
            auto& registry = m_demo_scene_ptr->get_registry();
            auto& stats = m_demo_scene_ptr->stats();

            ImGui::Text("Entities: %u", registry.storage<entt::entity>().in_use());
            ImGui::Separator();

            if (ImGui::Checkbox("Demo systems enabled (label: demo)", &stats.demo_systems_enabled))
            {
                if (auto* schedule = m_demo_scene_ptr->get_schedule())
                {
                    schedule->set_label_enabled("demo", stats.demo_systems_enabled);
                }
            }

            ImGui::Separator();
            ImGui::Text("Validation");
            ImGui::Text("Startup runs: %llu", static_cast<unsigned long long>(stats.startup_runs));
            ImGui::Text("Rotation runs: %llu", static_cast<unsigned long long>(stats.rotation_runs));
            ImGui::Text("Debug runs: %llu", static_cast<unsigned long long>(stats.debug_runs));
            ImGui::Text("Physics probe runs: %llu", static_cast<unsigned long long>(stats.physics_runs));
            ImGui::Text("Max rotation angle: %.2f", stats.max_rotation_angle);
            ImGui::Text("Dependency violations: %llu", static_cast<unsigned long long>(stats.dependency_violations));

            const bool validation_ok = stats.startup_runs >= 1
                && stats.dependency_violations == 0
                && (stats.demo_systems_enabled ? stats.rotation_runs > 0 : true);

            ImGui::TextColored(validation_ok ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f) : ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                               validation_ok ? "Validation: PASS" : "Validation: FAIL");

            if (auto* schedule = m_demo_scene_ptr->get_schedule())
            {
                ImGui::Separator();
                ImGui::Text("Systems: %zu", schedule->system_count());

                const auto& order = schedule->get_execution_order();
                for (size_t sys_id : order)
                {
                    const auto* sys = schedule->get_system_by_id(sys_id);
                    if (sys)
                    {
                        ImGui::Text("[%s] %s %s",
                                    schedule::stage_to_string(sys->stage),
                                    sys->name.c_str(),
                                    sys->enabled ? "" : "(disabled)");
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", Engine::get().statistics().FramesPerSecond);
        ImGui::Text("Frame Time: %.2f ms", Engine::get().statistics().FrameTime);

        if (ImGui::Button("Exit Demo"))
        {
            set_app_state(AppState::Closing);
        }

        ImGui::End();
    }

private:
    DemoScene* m_demo_scene_ptr = nullptr;
};

diverse::Application* diverse::createApplication()
{
    srand(static_cast<unsigned>(time(nullptr)));
    return new DemoApplication();
}
