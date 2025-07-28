#include "precompile.h"
#include "scene.h"
#include "engine/input.h"
#include "engine/application.h"
#include "camera/camera.h"
#include "component/environment.h"
#include "sun_controller.h"
#include "utility/time_step.h"

#include "events/event.h"
#include "events/application_event.h"

#include "maths/transform.h"
#include "engine/file_system.h"
#include "scene/component/components.h"
#include "scene/component/time_line.h"

#include "scene/entity_manager.h"
#include "scene/component/gaussian_component.h"
#include "scene/component/point_cloud_component.h"
#include "scene/component/mesh_model_component.h"
#include "scene/component/environment.h"
#include "scene/component/light/directional_light.h"
#include "scene/component/light/point_light.h"
#include "scene/component/light/rect_light.h"
#include "scene/component/light/spot_light.h"
#include "scene/component/light/disk_light.h"
#include "scene/component/light/cylinder_light.h"
#include "scene_graph.h"

#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <entt/entity/registry.hpp>
#include <sol/sol.hpp>

#pragma warning(push, 0)
// Legacy version of entt snapshot loaded to load older scene versions saved before updating entt
namespace entt
{

    /**
     * @brief Utility class to restore a snapshot as a whole.
     *
     * A snapshot loader requires that the destination registry be empty and loads
     * all the data at once while keeping intact the identifiers that the entities
     * originally had.<br/>
     * An example of use is the implementation of a save/restore utility.
     *
     * @tparam Entity A valid entity type (see entt_traits for more details).
     */
    template <typename Entity>
    class basic_snapshot_loader_legacy
    {
        /*! @brief A registry is allowed to create snapshot loaders. */
        friend class basic_registry<Entity>;

        using traits_type = entt_traits<Entity>;

        template <typename Type, typename Archive>
        void assign(Archive& archive) const
        {
            typename traits_type::entity_type length{};
            archive(length);

            entity_type entt{};

            if constexpr (std::is_empty_v<Type>)
            {
                while (length--)
                {
                    archive(entt);
                    const auto entity = reg->valid(entt) ? entt : reg->create(entt);
                    // ENTT_ASSERT(entity == entt);
                    reg->template emplace<Type>(entity);
                }
            }
            else
            {
                Type instance{};

                while (length--)
                {
                    archive(entt, instance);
                    const auto entity = reg->valid(entt) ? entt : reg->create(entt);
                    // ENTT_ASSERT(entity == entt);
                    reg->template emplace<Type>(entity, std::move(instance));
                }
            }
        }

    public:
        /*! @brief Underlying entity identifier. */
        using entity_type = Entity;

        /**
         * @brief Constructs an instance that is bound to a given registry.
         * @param source A valid reference to a registry.
         */
        basic_snapshot_loader_legacy(basic_registry<entity_type>& source) noexcept
            : reg{ &source }
        {
            // restoring a snapshot as a whole requires a clean registry
            // ENTT_ASSERT(reg->empty());
        }

        /*! @brief Default move constructor. */
        basic_snapshot_loader_legacy(basic_snapshot_loader_legacy&&) = default;

        /*! @brief Default move assignment operator. @return This loader. */
        basic_snapshot_loader_legacy& operator=(basic_snapshot_loader_legacy&&) = default;

        /**
         * @brief Restores entities that were in use during serialization.
         *
         * This function restores the entities that were in use during serialization
         * and gives them the versions they originally had.
         *
         * @tparam Archive Type of input archive.
         * @param archive A valid reference to an input archive.
         * @return A valid loader to continue restoring data.
         */
        template <typename Archive>
        const basic_snapshot_loader_legacy& entities(Archive& archive) const
        {
            typename traits_type::entity_type length{};

            archive(length);
            std::vector<entity_type> all(length);

            for (decltype(length) pos{}; pos < length; ++pos)
            {
                archive(all[pos]);
            }

            reg->assign(all.cbegin(), all.cend(), 0);

            return *this;
        }

        /**
         * @brief Restores components and assigns them to the right entities.
         *
         * The template parameter list must be exactly the same used during
         * serialization. In the event that the entity to which the component is
         * assigned doesn't exist yet, the loader will take care to create it with
         * the version it originally had.
         *
         * @tparam Component Types of components to restore.
         * @tparam Archive Type of input archive.
         * @param archive A valid reference to an input archive.
         * @return A valid loader to continue restoring data.
         */
        template <typename... Component, typename Archive>
        const basic_snapshot_loader_legacy& component(Archive& archive) const
        {
            (assign<Component>(archive), ...);
            return *this;
        }

        /**
         * @brief Destroys those entities that have no components.
         *
         * In case all the entities were serialized but only part of the components
         * was saved, it could happen that some of the entities have no components
         * once restored.<br/>
         * This functions helps to identify and destroy those entities.
         *
         * @return A valid loader to continue restoring data.
         */
        const basic_snapshot_loader_legacy& orphans() const
        {
            /*        reg->orphans([this](const auto entt) {
                        reg->destroy(entt);
                        });*/
            DS_LOG_WARN("May need to fix this - basic_snapshot_loader_legacy::orphans()");

            return *this;
        }

    private:
        basic_registry<entity_type>* reg;
    };
}
#pragma warning(pop)

namespace diverse
{
    Scene::Scene(const std::string& name)
        : m_SceneName(name)
        , m_ScreenWidth(0)
        , m_ScreenHeight(0)
    {
        m_EntityManager = createUniquePtr<EntityManager>(this);
        m_EntityManager->add_dependency<Camera, maths::Transform>();
        m_EntityManager->add_dependency<GaussianComponent, maths::Transform>();
        m_EntityManager->add_dependency<PointCloudComponent, maths::Transform>();
        m_EntityManager->add_dependency<MeshModelComponent, maths::Transform>();
        m_EntityManager->add_dependency<DirectionalLightComponent, maths::Transform>();
        m_EntityManager->add_dependency<PointLightComponent, maths::Transform>();
        m_EntityManager->add_dependency<SpotLightComponent, maths::Transform>();
        m_EntityManager->add_dependency<RectLightComponent, maths::Transform>();
        m_EntityManager->add_dependency<DiskLightComponent, maths::Transform>();
        m_EntityManager->add_dependency<CylinderLightComponent, maths::Transform>();
        m_SceneGraph = createUniquePtr<SceneGraph>();
        m_SceneGraph->init(m_EntityManager->get_registry());
    }

    Scene::~Scene()
    {
        m_EntityManager->clear();
    }

    entt::registry& Scene::get_registry()
    {
        return m_EntityManager->get_registry();
    }

    void Scene::on_init()
    {
        DS_PROFILE_FUNCTION();
    }

    void Scene::on_cleanup_scene()
    {
        DS_PROFILE_FUNCTION();
        delete_all_game_objects();
    };

    void Scene::delete_all_game_objects()
    {
        DS_PROFILE_FUNCTION();
        m_EntityManager->clear();
    }

    void Scene::on_update(const TimeStep& timeStep)
    {
        DS_PROFILE_FUNCTION();
        const glm::vec2& mousePos = Input::get().get_mouse_position();

        auto defaultCameraControllerView = m_EntityManager->get_entities_with_type<DefaultCameraController>();
        auto cameraView = m_EntityManager->get_entities_with_type<Camera>();
        Camera* camera = nullptr;
        maths::Transform* camera_transform = nullptr;
        if (!cameraView.empty())
        {
            camera = &cameraView.front().get_component<Camera>();
            camera_transform = &cameraView.front().get_component<maths::Transform>();
        }

        if (!defaultCameraControllerView.empty())
        {
            auto& cameraController = defaultCameraControllerView.front().get_component<DefaultCameraController>();
            camera_transform = defaultCameraControllerView.front().try_get_component<maths::Transform>();
            if (Application::get().get_scene_active() && camera_transform && cameraController.get_controller())
            {
                cameraController.get_controller()->set_camera(camera);
                cameraController.get_controller()->handle_mouse(*camera_transform, (float)timeStep.get_seconds(), mousePos.x, mousePos.y);
                cameraController.get_controller()->handle_keyboard(*camera_transform, (float)timeStep.get_seconds());
            }
        }

        auto environmentView = m_EntityManager->get_entities_with_type<Environment>();
        Environment* enviroment = nullptr;
        if (!environmentView.empty())
        {
            enviroment = &environmentView.front().get_component<Environment>();
        }
        if (Application::get().get_scene_active() && enviroment && camera_transform)
        {
            auto& input = Input::get();
            if (input.get_mouse_held(InputCode::ButtonLeft) && enviroment->mode == Environment::Mode::SunSky)
            {
                auto delta = input.get_mouse_delta();
                auto delta_x = (delta.x / (f32)2048) * 6.283;
                auto delta_y = (delta.y / (f32)968) * 3.1415;
                glm::quat ref_frame(camera_transform->get_world_orientation().w, 0.0f, camera_transform->get_world_orientation().y, 0.0f);
                ref_frame = glm::normalize(ref_frame);
                auto& sun_controller = environmentView.front().get_component<SunController>();
                sun_controller.view_space_rotate(ref_frame, delta_x, delta_y);
            }
        }

        m_SceneGraph->update(m_EntityManager->get_registry());

    }

    void Scene::on_event(Event& e)
    {
        DS_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Scene::on_window_resize));
    }

    Entity Scene::get_keyFrame_entity()
    {
        auto keyFrameView = m_EntityManager->get_entities_with_type<KeyFrameTimeLine>();
        if (!keyFrameView.empty())
        {
            return keyFrameView.front();
        }
        return Entity();
    }

    bool Scene::on_window_resize(WindowResizeEvent& e)
    {
        DS_PROFILE_FUNCTION();
        if (!Application::get().get_scene_active())
            return false;

        auto cameraView = m_EntityManager->get_registry().view<Camera>();
        if (!cameraView.empty())
        {
            m_EntityManager->get_registry().get<Camera>(cameraView.front()).set_aspect_ratio(static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight()));
        }

        return false;
    }

    void Scene::set_screen_size(uint32_t width, uint32_t height)
    {
        m_ScreenWidth = width;
        m_ScreenHeight = height;

        auto cameraView = m_EntityManager->get_registry().view<Camera>();
        if (!cameraView.empty())
        {
            m_EntityManager->get_registry().get<Camera>(cameraView.front()).set_aspect_ratio(static_cast<float>(m_ScreenWidth) / static_cast<float>(m_ScreenHeight));
        }
    }

    maths::BoundingBox  Scene::get_world_bounding_box() const   
    {
        auto group = m_EntityManager->get_registry().group<GaussianComponent>(entt::get<maths::Transform>);
        maths::BoundingBox  box;
        for (auto entity : group)
        {
            const auto& [model, trans] = group.get<GaussianComponent, maths::Transform>(entity);
            auto& worldTransform = trans.get_world_matrix();
            auto bbCopy = model.ModelRef->get_world_bounding_box(worldTransform);
            box.merge(bbCopy);
        }
        auto model_group = m_EntityManager->get_registry().group<MeshModelComponent>(entt::get<maths::Transform>);
        for (auto entity : model_group)
        {
            const auto& [model, trans] = model_group.get<MeshModelComponent, maths::Transform>(entity);
            auto& worldTransform = trans.get_world_matrix();
            auto bbCopy = model.ModelRef->get_world_bounding_box(worldTransform);
            box.merge(bbCopy);
        }
        return box;
    }


#define ALL_COMPONENTSV1 maths::Transform, NameComponent, ActiveComponent, Hierarchy, Camera, DefaultCameraController, GaussianComponent, KeyFrameTimeLine, Environment,MeshModelComponent,PointCloudComponent

#define ALL_COMPONENTSENTTV1(input) get<maths::Transform>(input)    \
                                    .get<NameComponent>(input)      \
                                    .get<ActiveComponent>(input)    \
                                    .get<Hierarchy>(input)          \
                                    .get<Camera>(input)             \
                                    .get<DefaultCameraController>(input)    \
                                    .get<GaussianComponent>(input)          \
                                    .get<PointCloudComponent>(input)          \
                                    .get<KeyFrameTimeLine>(input).get<Environment>(input).get<MeshModelComponent>(input)

    void Scene::serialise(const std::string& filePath, bool binary)
    {
        DS_PROFILE_FUNCTION();
        DS_LOG_INFO("Scene saved - {0}", filePath);
        std::string path = filePath;
        path += m_SceneName; // stringutility::remove_spaces(m_SceneName);

        m_SceneSerialisationVersion = SceneSerialisationVersion;

        if (binary)
        {
            path += std::string(".bin");

            std::ofstream file(path, std::ios::binary);

            {
                // output finishes flushing its contents when it goes out of scope
                cereal::BinaryOutputArchive output{ file };
                output(*this);
                auto& snap = entt::snapshot{ m_EntityManager->get_registry() }.get<entt::entity>(output).ALL_COMPONENTSENTTV1(output);
                Application::get().save_custom(output, snap);
            }
            file.close();
        }
        else
        {
            std::stringstream storage;
            path += std::string(".dsn");

            {
                // output finishes flushing its contents when it goes out of scope
                cereal::JSONOutputArchive output{ storage };
                output(*this);
                auto& snap = entt::snapshot{ m_EntityManager->get_registry() }.get<entt::entity>(output).ALL_COMPONENTSENTTV1(output);
                Application::get().save_custom(output, snap);
            }
            FileSystem::write_text_file(path, storage.str());
        }
        has_serialised = true;
    }

    void Scene::deserialise(const std::string& filePath, bool binary)
    {
        DS_PROFILE_FUNCTION();
        m_EntityManager->clear();
        m_SceneGraph->disable_on_construct(true, m_EntityManager->get_registry());
        std::string path = filePath;
        path += m_SceneName; // stringutility::remove_spaces(m_SceneName);
        if (binary)
        {
            path += std::string(".bin");

            if (!FileSystem::file_exists(path))
            {
                DS_LOG_ERROR("No saved scene file found {0}", path);
                return;
            }
            try
            {
                std::ifstream file(path, std::ios::binary);
                cereal::BinaryInputArchive input(file);
                input(*this);

                auto& entsnap = entt::snapshot_loader{ m_EntityManager->get_registry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV1(input);
                Application::get().load_custom(input, entsnap);
            }
            catch (...)
            {
                DS_LOG_ERROR("Failed to load scene - {0}", path);
            }
        }
        else
        {
            path += std::string(".dsn");

            if (!FileSystem::file_exists(path))
            {
                DS_LOG_ERROR("No saved scene file found {0}", path);
                return;
            }
            try
            {
                std::string data = FileSystem::read_text_file(path);
                std::istringstream istr;
                istr.str(data);
                cereal::JSONInputArchive input(istr);
                input(*this);
                auto& entsnap = entt::snapshot_loader{ m_EntityManager->get_registry() }.get<entt::entity>(input).ALL_COMPONENTSENTTV1(input);
                Application::get().load_custom(input, entsnap);
            }
            catch (...)
            {
                DS_LOG_ERROR("Failed to load scene - {0}", path);
            }
        }
        m_SceneGraph->disable_on_construct(false, m_EntityManager->get_registry());
        Application::get().handle_new_scene(this);
        has_serialised = true;
    }

    void Scene::update_scene_graph()
    {
        DS_PROFILE_FUNCTION();
        m_SceneGraph->update(m_EntityManager->get_registry());
    }

    template <typename T>
    static void CopyComponentIfExists(entt::entity dst, entt::entity src, entt::registry& registry)
    {
        if (registry.all_of<T>(src))
        {
            auto& srcComponent = registry.get<T>(src);
            registry.emplace_or_replace<T>(dst, srcComponent);
        }
    }

    template <typename... Component>
    static void CopyEntity(entt::entity dst, entt::entity src, entt::registry& registry)
    {
        (CopyComponentIfExists<Component>(dst, src, registry), ...);
    }

    Entity Scene::create_entity()
    {
        return m_EntityManager->create();
    }

    Entity Scene::create_entity(const std::string& name)
    {
        DS_PROFILE_FUNCTION();
        return m_EntityManager->create(name);
    }

    Entity Scene::get_entity_by_uuid(uint64_t id)
    {
        DS_PROFILE_FUNCTION();
        return m_EntityManager->get_entity_by_uuid(id);
    }

    namespace
    {
        void _DestroyEntity(entt::entity entity, entt::registry& registry)
        {
            DS_PROFILE_FUNCTION();
            auto hierarchyComponent = registry.try_get<Hierarchy>(entity);
            if (hierarchyComponent)
            {
                entt::entity child = hierarchyComponent->first();
                while (child != entt::null)
                {
                    auto hierarchyComponent = registry.try_get<Hierarchy>(child);
                    auto next = hierarchyComponent ? hierarchyComponent->next() : entt::null;
                    _DestroyEntity(child, registry);
                    child = next;
                }
            }
            registry.destroy(entity);
        }
    }

    void Scene::destroy_entity(Entity entity)
    {
        _DestroyEntity(entity.get_handle(), get_registry());
    }

    void Scene::duplicate_entity(Entity entity)
    {
        DS_PROFILE_FUNCTION();
        duplicate_entity(entity, Entity(entt::null, nullptr));
    }

    void Scene::duplicate_entity(Entity entity, Entity parent)
    {
        DS_PROFILE_FUNCTION();
        m_SceneGraph->disable_on_construct(true, m_EntityManager->get_registry());

        Entity newEntity = m_EntityManager->create();

        CopyEntity<ALL_COMPONENTSV1>(newEntity.get_handle(), entity.get_handle(), m_EntityManager->get_registry());
        newEntity.get_component<IDComponent>().ID = UUID();

        auto hierarchyComponent = newEntity.try_get_component<Hierarchy>();
        if(hierarchyComponent)
        {
            hierarchyComponent->m_First  = entt::null;
            hierarchyComponent->m_Parent = entt::null;
            hierarchyComponent->m_Next   = entt::null;
            hierarchyComponent->m_Prev   = entt::null;
        }

        auto children  = entity.get_children_temp();
        u32 childCount = entity.get_child_count();

        for(u32 i = 0; i < childCount; i++)
        {
            duplicate_entity(children[i], newEntity);
        }

        if(parent)
            newEntity.set_parent(parent);

        m_SceneGraph->disable_on_construct(false, m_EntityManager->get_registry());
    }

}
