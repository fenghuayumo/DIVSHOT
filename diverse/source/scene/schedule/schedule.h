#pragma once

#include "core/reference.h"
#include "system_descriptor.h"
#include "world_context.h"
#include "graph.h"
#include "query.h"
#include "res.h"
#include "system_condition.h"

#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <cstddef>
#include <unordered_set>

// Forward declarations
namespace diverse
{
    class TimeStep;
    class Scene;
}

namespace diverse::System
{
    namespace JobSystem
    {
        struct Context;
    }
}

namespace diverse::schedule
{

    // Forward declaration
    class Schedule;

    /**
     * @brief Plugin for organizing systems into reusable groups.
     *
     * Plugins allow grouping related systems and configurations
     * for easy reuse across different applications.
     */
    class Plugin
    {
    public:
        virtual ~Plugin() = default;

        /**
         * @brief Build the plugin into the schedule
         *
         * @param schedule The schedule to add systems to
         */
        virtual void build(Schedule& schedule) = 0;

        /**
         * @brief Get the plugin name
         */
        virtual const char* name() const = 0;
    };

    /**
     * @brief System execution configuration returned by add_system
     */
    class SystemConfig
    {
    public:
        SystemConfig(size_t system_id, Schedule* schedule)
            : m_system_id(system_id)
            , m_schedule(schedule)
        {
        }

        /**
         * @brief Specify that this system must run before another
         */
        SystemConfig& before(const std::string& system_name);

        /**
         * @brief Specify that this system must run after another
         */
        SystemConfig& after(const std::string& system_name);

        /**
         * @brief Mark this system as thread-local (must run on main thread)
         */
        SystemConfig& set_thread_local_flag();

        /**
         * @brief Add a label to this system
         */
        SystemConfig& add_label(const std::string& label);

        /**
         * @brief Set the execution stage for this system
         */
        SystemConfig& in_stage(SystemStage stage);

        /**
         * @brief Add a run condition to this system
         */
        SystemConfig& run_if(std::shared_ptr<ISystemCondition> condition);

        /**
         * @brief Add a function-based run condition
         */
        SystemConfig& run_if(std::function<bool(WorldContext*)> condition_fn);

    private:
        size_t m_system_id;
        Schedule* m_schedule;
    };

    /**
     * @brief Main scheduler for system execution.
     *
     * Manages system registration, dependency resolution, and execution.
     * Supports both sequential and parallel execution modes.
     */
    class Schedule
    {
        friend class SystemConfig;  // Allow SystemConfig to access private methods
    public:
    public:
        explicit Schedule(Scene* scene);
        ~Schedule();

        // Non-copyable
        Schedule(const Schedule&) = delete;
        Schedule& operator=(const Schedule&) = delete;

        /**
         * @brief Add a function-based system
         *
         * @param name Unique name for the system
         * @param system_fn Function to execute each frame
         * @return SystemConfig for further configuration
         */
        SystemConfig add_system(const std::string& name, std::function<void()> system_fn);

        /**
         * @brief Add a system with WorldContext parameter
         *
         * The system function will receive WorldContext for accessing registry, scene, etc.
         */
        SystemConfig add_system(const std::string& name, std::function<void(WorldContext*)> system_fn);

        /**
         * @brief Add a dependency between two systems by name
         *
         * @param first System name that must execute first
         * @param second System name that depends on first
         * @return true if dependency was added successfully
         */
        bool add_dependency(const std::string& first, const std::string& second);

        /**
         * @brief Mark a system as thread-local
         *
         * @param system_name Name of the system
         * @param local true if system must run on main thread
         * @return true if system was found and updated
         */
        bool set_thread_local(const std::string& system_name, bool local = true);

        /**
         * @brief Build the dependency graph
         *
         * Must be called before execute(). Returns false if a cycle is detected.
         *
         * @return true if build succeeded, false otherwise
         */
        bool build();

        /**
         * @brief Execute all systems sequentially
         *
         * @param timestep Delta time for this frame
         */
        void execute(const TimeStep& timestep);

        /**
         * @brief Execute all systems with parallel processing
         *
         * Uses JobSystem to parallelize independent systems.
         *
         * @param job_ctx JobSystem context for parallel execution
         * @param timestep Delta time for this frame
         */
        void execute_parallel(System::JobSystem::Context& job_ctx, const TimeStep& timestep);

        /**
         * @brief Get a system descriptor by name
         *
         * @return Pointer to descriptor, or nullptr if not found
         */
        SystemDescriptor* get_system(const std::string& name);

        /**
         * @brief Get a system descriptor by ID
         *
         * @return Pointer to descriptor, or nullptr if not found
         */
        SystemDescriptor* get_system_by_id(size_t id);
        const SystemDescriptor* get_system_by_id(size_t id) const;

        /**
         * @brief Check if a system exists
         */
        bool has_system(const std::string& name) const;

        /**
         * @brief Enable or disable a system at runtime
         *
         * @return true if system was found and updated
         */
        bool set_system_enabled(const std::string& name, bool enabled);

        /**
         * @brief Remove a system
         *
         * @return true if system was found and removed
         */
        bool remove_system(const std::string& name);

        /**
         * @brief Clear all systems
         */
        void clear();

        /**
         * @brief Get the number of registered systems
         */
        size_t system_count() const { return m_systems.size(); }

        /**
         * @brief Check if the schedule has been built
         */
        bool is_built() const { return m_is_built; }

        /**
         * @brief Get the execution order (after build)
         */
        const std::vector<size_t>& get_execution_order() const;

        /**
         * @brief Get the scene
         */
        Scene* scene() const { return m_scene; }

        /**
         * @brief Add a plugin to the schedule
         */
        void add_plugin(std::shared_ptr<Plugin> plugin);

        /**
         * @brief Enable/disable all systems with a specific label
         */
        void set_label_enabled(const std::string& label, bool enabled);

        /**
         * @brief Get all systems with a specific label
         */
        std::vector<SystemDescriptor*> get_systems_with_label(const std::string& label);

        /**
         * @brief Execute systems in a specific stage
         */
        void execute_stage(SystemStage stage, const TimeStep& timestep);

        /**
         * @brief Execute all startup stages (once)
         */
        void execute_startup_stages(const TimeStep& timestep);

        /**
         * @brief Check if startup stages have been executed
         */
        bool has_executed_startup() const { return m_has_executed_startup; }

    private:
        /**
         * @brief Get system ID by name
         */
        size_t get_system_id(const std::string& name) const;

        /**
         * @brief Get system name by ID
         */
        const std::string& get_system_name(size_t id) const;

        /**
         * @brief Execute a single system
         */
        void execute_system(SystemDescriptor& system, const TimeStep& timestep);

        /**
         * @brief Helper methods for SystemConfig
         */
        void add_label_to_system(const std::string& system_name, const std::string& label);
        void set_system_stage(const std::string& system_name, SystemStage stage);
        void add_condition_to_system(const std::string& system_name, std::shared_ptr<ISystemCondition> condition);

        Scene* m_scene;
        std::unordered_map<std::string, size_t> m_name_to_id;
        std::unordered_map<size_t, std::string> m_id_to_name;
        std::unordered_map<size_t, SystemDescriptor> m_systems;
        UniquePtr<DependencyGraph> m_graph;
        UniquePtr<WorldContext> m_world_context;
        size_t m_next_system_id = 1;
        bool m_is_built = false;
        bool m_has_executed_startup = false;
        std::vector<std::shared_ptr<Plugin>> m_plugins;
    };

} // namespace diverse::schedule
