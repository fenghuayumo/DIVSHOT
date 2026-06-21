#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <cstddef>
#include <memory>
#include "system_condition.h"

namespace diverse::schedule
{

    enum class SystemKind
    {
        Function,  // Lambda/function-based system
        Class      // ISystem-based (legacy support)
    };

    /**
     * @brief Forward declaration
     */
    class ISystemCondition;

    /**
     * @brief Metadata descriptor for a system in the schedule.
     *
     * Contains information about system execution constraints,
     * dependencies, labels, stages, and component access patterns.
     */
    struct SystemDescriptor
    {
        std::string name;
        SystemKind kind = SystemKind::Function;
        size_t system_id = 0;  // Unique identifier for this system

        // Dependency tracking
        std::vector<size_t> before;      // System IDs that must run after this system
        std::vector<size_t> after;       // System IDs that this system must run after
        std::vector<size_t> depends_on;  // Explicit dependencies

        // Execution constraints
        bool is_thread_local = false;  // Must run on main thread
        bool enabled = true;           // Can be disabled at runtime

        // Component access patterns (for parallel execution validation)
        std::unordered_set<size_t> read_components;
        std::unordered_set<size_t> write_components;

        // Labels and organization
        std::vector<std::string> labels;  // Labels for grouping systems
        SystemStage stage = SystemStage::Update;  // Execution stage

        // Run conditions
        std::vector<std::shared_ptr<ISystemCondition>> conditions;  // Conditions that must be met

        // Execution function
        std::function<void()> execute_fn;

        SystemDescriptor() = default;

        SystemDescriptor(const std::string& name_, size_t id)
            : name(name_), system_id(id)
        {
        }

        /**
         * @brief Check if this system can run in parallel with another
         *
         * Two systems can run in parallel if they don't have conflicting
         * component access (no write-write or read-write conflicts).
         */
        bool can_run_parallel_with(const SystemDescriptor& other) const
        {
            // Check write-write conflicts
            for (size_t comp : write_components)
            {
                if (other.write_components.count(comp) > 0)
                    return false;
            }

            // Check read-write conflicts
            for (size_t comp : write_components)
            {
                if (other.read_components.count(comp) > 0)
                    return false;
            }

            for (size_t comp : read_components)
            {
                if (other.write_components.count(comp) > 0)
                    return false;
            }

            // Also check dependency constraints
            for (size_t dep : depends_on)
            {
                if (dep == other.system_id)
                    return false;
            }

            for (size_t dep : other.depends_on)
            {
                if (dep == system_id)
                    return false;
            }

            return true;
        }

        /**
         * @brief Add a component type that this system reads
         */
        template<typename T>
        void reads()
        {
            read_components.insert(typeid(T).hash_code());
        }

        /**
         * @brief Add a component type that this system writes
         */
        template<typename T>
        void writes()
        {
            write_components.insert(typeid(T).hash_code());
        }
    };

} // namespace diverse::schedule
