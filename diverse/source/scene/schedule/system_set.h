#pragma once

#include "schedule.h"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

namespace diverse::schedule
{

    /**
     * @brief Logical grouping of systems.
     *
     * SystemSets allow organizing systems into groups that can be
     * configured and executed together. Useful for organizing systems
     * by functionality (e.g., PhysicsSet, RenderingSet).
     *
     * Note: SystemSet acts as a deferred configuration - systems are
     * not added to the schedule until apply_to() is called.
     */
    class SystemSet
    {
    public:
        using ConfigFn = std::function<void(Schedule&)>;

        /**
         * @brief Create a named system set
         *
         * @param name Unique name for this set
         */
        explicit SystemSet(const std::string& name)
            : m_name(name)
            , m_schedule(nullptr)
        {
        }

        ~SystemSet() = default;

        /**
         * @brief Add a system function to this set
         *
         * @param name Unique name for the system
         * @param system_fn Function to execute each frame
         * @return SystemConfig for further configuration (deferred until apply_to)
         */
        SystemConfig add_system(const std::string& name, std::function<void()> system_fn)
        {
            m_system_configs.push_back([name, system_fn](Schedule& schedule) {
                schedule.add_system(name, system_fn);
            });
            // Store the name for deferred configuration
            m_system_names.push_back(name);
            // Return a placeholder config that will be resolved during apply_to
            return SystemConfig(0, this);
        }

        /**
         * @brief Add a system with WorldContext parameter
         */
        SystemConfig add_system(const std::string& name, std::function<void(WorldContext*)> system_fn)
        {
            m_system_configs.push_back([name, system_fn](Schedule& schedule) {
                schedule.add_system(name, system_fn);
            });
            m_system_names.push_back(name);
            return SystemConfig(0, this);
        }

        /**
         * @brief Configure the systems in this set
         *
         * @param config Function that receives the Schedule and can configure
         *               dependencies, thread_local settings, etc.
         * @return Reference to this set for chaining
         */
        SystemSet& configure(ConfigFn config)
        {
            m_configs.push_back(std::move(config));
            return *this;
        }

        /**
         * @brief Apply this set to a schedule
         *
         * Adds all systems and configurations to the given schedule.
         *
         * @param schedule The schedule to apply this set to
         */
        void apply_to(Schedule& schedule)
        {
            m_schedule = &schedule;

            // Add all systems
            for (const auto& config : m_system_configs)
            {
                config(schedule);
            }

            // Apply configurations
            for (const auto& config : m_configs)
            {
                config(schedule);
            }
        }

        /**
         * @brief Get the name of this set
         */
        const std::string& name() const { return m_name; }

        /**
         * @brief Get the number of systems in this set
         */
        size_t system_count() const { return m_system_configs.size(); }

        /**
         * @brief Get the schedule (valid after apply_to is called)
         */
        Schedule* schedule() const { return m_schedule; }

    private:
        std::string m_name;
        std::vector<ConfigFn> m_system_configs;  // System registration functions
        std::vector<ConfigFn> m_configs;         // Configuration functions
        std::vector<std::string> m_system_names;  // System names for deferred config
        Schedule* m_schedule;                    // Schedule after apply_to
    };

    /**
     * @brief Helper function to create a system set
     */
    inline SystemSet make_system_set(const std::string& name)
    {
        return SystemSet(name);
    }

} // namespace diverse::schedule
