#pragma once

#include <any>
#include <unordered_map>
#include <cstddef>

// Forward declarations
namespace diverse
{

    class TimeStep;
    class Scene;

}

namespace diverse::schedule
{

    // Forward declaration for EnTT registry
    class Registry;

    /**
     * @brief Shared context passed to all systems during execution.
     *
     * Provides access to the EnTT registry, scene, time step,
     * and global resources. Systems receive this context through
     * their parameters (Query, Res, etc.).
     */
    class WorldContext
    {
    public:
        explicit WorldContext(Scene* scene, void* registry, TimeStep* timestep)
            : m_scene(scene)
            , m_registry(registry)
            , m_timestep(timestep)
        {
        }

        ~WorldContext() = default;

        /**
         * @brief Get the EnTT registry (as void* to avoid include issues)
         */
        void* registry() { return m_registry; }
        const void* registry() const { return m_registry; }

        /**
         * @brief Get the scene
         */
        Scene* scene() { return m_scene; }
        const Scene* scene() const { return m_scene; }

        /**
         * @brief Get the current time step
         */
        TimeStep* timestep() { return m_timestep; }
        const TimeStep* timestep() const { return m_timestep; }

        /**
         * @brief Insert a global resource
         *
         * Resources are singletons that can be accessed by any system.
         * If a resource of the same type already exists, it will be replaced.
         */
        template<typename T>
        void insert_resource(T&& resource)
        {
            m_resources[typeid(T).hash_code()] = std::forward<T>(resource);
        }

        /**
         * @brief Get a global resource
         *
         * Returns nullptr if the resource doesn't exist.
         */
        template<typename T>
        T* get_resource()
        {
            auto it = m_resources.find(typeid(T).hash_code());
            if (it != m_resources.end())
            {
                return std::any_cast<T>(&it->second);
            }
            return nullptr;
        }

        /**
         * @brief Get a global resource (const version)
         */
        template<typename T>
        const T* get_resource() const
        {
            auto it = m_resources.find(typeid(T).hash_code());
            if (it != m_resources.end())
            {
                return std::any_cast<T>(&it->second);
            }
            return nullptr;
        }

        /**
         * @brief Check if a resource exists
         */
        template<typename T>
        bool has_resource() const
        {
            return m_resources.count(typeid(T).hash_code()) > 0;
        }

        /**
         * @brief Remove a resource
         */
        template<typename T>
        void remove_resource()
        {
            m_resources.erase(typeid(T).hash_code());
        }

        /**
         * @brief Clear all resources
         */
        void clear_resources()
        {
            m_resources.clear();
        }

        /**
         * @brief Update the context pointers for a new frame
         *
         * This is more efficient than creating a new WorldContext each frame.
         */
        void update(Scene* scene, void* registry, TimeStep* timestep)
        {
            m_scene = scene;
            m_registry = registry;
            m_timestep = timestep;
        }

    private:
        Scene* m_scene = nullptr;
        void* m_registry = nullptr;
        TimeStep* m_timestep = nullptr;

        // Global resources storage
        std::unordered_map<size_t, std::any> m_resources;
    };

} // namespace diverse::schedule
