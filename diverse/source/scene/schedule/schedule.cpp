#include "core/reference.h"
#include "core/core.h"
#include "core/ds_log.h"
#include "core/job_system.h"

#include "schedule.h"
#include "scene/scene.h"

#include <stdexcept>

namespace diverse::schedule
{

    // SystemConfig implementation
    SystemConfig& SystemConfig::before(const std::string& system_name)
    {
        if (m_schedule)
        {
            m_schedule->add_dependency(m_schedule->get_system_name(m_system_id), system_name);
        }
        return *this;
    }

    SystemConfig& SystemConfig::after(const std::string& system_name)
    {
        if (m_schedule)
        {
            m_schedule->add_dependency(system_name, m_schedule->get_system_name(m_system_id));
        }
        return *this;
    }

    SystemConfig& SystemConfig::set_thread_local_flag()
    {
        if (m_schedule)
        {
            m_schedule->set_thread_local(m_schedule->get_system_name(m_system_id), true);
        }
        return *this;
    }

    SystemConfig& SystemConfig::add_label(const std::string& label)
    {
        if (m_schedule)
        {
            m_schedule->add_label_to_system(m_schedule->get_system_name(m_system_id), label);
        }
        return *this;
    }

    SystemConfig& SystemConfig::in_stage(SystemStage stage)
    {
        if (m_schedule)
        {
            m_schedule->set_system_stage(m_schedule->get_system_name(m_system_id), stage);
        }
        return *this;
    }

    SystemConfig& SystemConfig::run_if(std::shared_ptr<ISystemCondition> condition)
    {
        if (m_schedule)
        {
            m_schedule->add_condition_to_system(m_schedule->get_system_name(m_system_id), condition);
        }
        return *this;
    }

    SystemConfig& SystemConfig::run_if(std::function<bool(WorldContext*)> condition_fn)
    {
        if (m_schedule)
        {
            auto condition = std::make_shared<FunctionCondition>(condition_fn);
            m_schedule->add_condition_to_system(m_schedule->get_system_name(m_system_id), condition);
        }
        return *this;
    }

    // Schedule implementation
    Schedule::Schedule(Scene* scene)
        : m_scene(scene)
        , m_graph(createUniquePtr<DependencyGraph>())
    {
        // WorldContext will be initialized with proper pointers during execute()
        m_world_context = createUniquePtr<WorldContext>(nullptr, nullptr, nullptr);
    }

    Schedule::~Schedule()
    {
        clear();
    }

    SystemConfig Schedule::add_system(const std::string& name, std::function<void()> system_fn)
    {
        if (has_system(name))
        {
            DS_LOG_WARN("System with name '{}' already exists. Replacing.", name);
            remove_system(name);
        }

        size_t id = m_next_system_id++;
        SystemDescriptor descriptor(name, id);
        descriptor.execute_fn = std::move(system_fn);

        m_systems[id] = std::move(descriptor);
        m_name_to_id[name] = id;
        m_id_to_name[id] = name;

        m_is_built = false;

        return SystemConfig(id, this);
    }

    SystemConfig Schedule::add_system(const std::string& name, std::function<void(WorldContext*)> system_fn)
    {
        // Wrap WorldContext parameter in a lambda that captures the context
        return add_system(name, [this, system_fn]() {
            system_fn(m_world_context.get());
        });
    }

    bool Schedule::add_dependency(const std::string& first, const std::string& second)
    {
        if (!has_system(first) || !has_system(second))
        {
            DS_LOG_ERROR("Cannot add dependency: missing system '{}' or '{}'", first, second);
            return false;
        }

        size_t first_id = get_system_id(first);
        size_t second_id = get_system_id(second);

        m_systems[first_id].before.push_back(second_id);
        m_systems[second_id].after.push_back(first_id);

        m_is_built = false;

        return true;
    }

    bool Schedule::set_thread_local(const std::string& system_name, bool local)
    {
        auto it = m_name_to_id.find(system_name);
        if (it == m_name_to_id.end())
        {
            return false;
        }

        m_systems[it->second].is_thread_local = local;
        return true;
    }

    bool Schedule::build()
    {
        // Clear and rebuild the graph
        m_graph->clear();

        // Add all systems to the graph
        for (auto& [id, descriptor] : m_systems)
        {
            m_graph->add_node(id, descriptor);

            // Add dependencies
            for (size_t dep_id : descriptor.after)
            {
                m_graph->add_dependency(dep_id, id);
            }
        }

        // Build the graph (validates and sorts)
        if (!m_graph->build())
        {
            DS_LOG_ERROR("Failed to build schedule: cycle detected in system dependencies");
            return false;
        }

        m_is_built = true;

        // Log execution order
        const auto& order = m_graph->get_execution_order();
        DS_LOG_INFO("Schedule built successfully with {} systems:", order.size());
        for (size_t i = 0; i < order.size(); ++i)
        {
            size_t sys_id = order[i];
            const auto& descriptor = m_systems[sys_id];
            DS_LOG_INFO("  [{}]: {} {}", i + 1, descriptor.name,
                       descriptor.is_thread_local ? "(thread-local)" : "");
        }

        return true;
    }

    void Schedule::execute(const TimeStep& timestep)
    {
        if (!m_is_built)
        {
            if (!build())
            {
                DS_LOG_ERROR("Cannot execute schedule: build failed");
                return;
            }
        }

        // Update WorldContext with current frame data
        m_world_context->update(
            m_scene,
            static_cast<void*>(&m_scene->get_registry()),
            const_cast<TimeStep*>(&timestep)
        );

        // Execute systems in order
        const auto& order = m_graph->get_execution_order();
        for (size_t sys_id : order)
        {
            auto& descriptor = m_systems[sys_id];
            if (descriptor.enabled)
            {
                // Check run conditions
                bool should_run = true;
                for (const auto& condition : descriptor.conditions)
                {
                    if (!condition->evaluate(m_world_context.get()))
                    {
                        should_run = false;
                        break;
                    }
                }

                if (should_run)
                {
                    execute_system(descriptor, timestep);
                }
            }
        }
    }

    void Schedule::execute_parallel(System::JobSystem::Context& job_ctx, const TimeStep& timestep)
    {
        if (!m_is_built)
        {
            if (!build())
            {
                DS_LOG_ERROR("Cannot execute schedule: build failed");
                return;
            }
        }

        // Update WorldContext with current frame data
        m_world_context->update(
            m_scene,
            static_cast<void*>(&m_scene->get_registry()),
            const_cast<TimeStep*>(&timestep)
        );

        // Execute systems by parallel stages
        const auto& stages = m_graph->get_parallel_stages();

        for (const auto& stage : stages)
        {
            // Separate thread-local and parallel systems
            std::vector<SystemDescriptor*> thread_local_systems;
            std::vector<SystemDescriptor*> parallel_systems;

            for (size_t sys_id : stage)
            {
                auto& descriptor = m_systems[sys_id];
                if (descriptor.enabled)
                {
                    // Check run conditions
                    bool should_run = true;
                    for (const auto& condition : descriptor.conditions)
                    {
                        if (!condition->evaluate(m_world_context.get()))
                        {
                            should_run = false;
                            break;
                        }
                    }

                    if (should_run)
                    {
                        if (descriptor.is_thread_local)
                        {
                            thread_local_systems.push_back(&descriptor);
                        }
                        else
                        {
                            parallel_systems.push_back(&descriptor);
                        }
                    }
                }
            }

            // Execute parallel systems using JobSystem
            if (!parallel_systems.empty())
            {
                // Capture data for lambda (using this pointer and timestep)
                auto* this_ptr = this;
                auto* timestep_ptr = &timestep;

                System::JobSystem::dispatch(
                    job_ctx,
                    static_cast<uint32_t>(parallel_systems.size()),
                    1,  // One job per system
                    [this_ptr, timestep_ptr, &parallel_systems](JobDispatchArgs args) {
                        if (args.jobIndex < parallel_systems.size())
                        {
                            this_ptr->execute_system(*parallel_systems[args.jobIndex], *timestep_ptr);
                        }
                    }
                );

                // Wait for parallel jobs to complete
                System::JobSystem::wait(job_ctx);
            }

            // Execute thread-local systems sequentially
            for (auto* descriptor : thread_local_systems)
            {
                execute_system(*descriptor, timestep);
            }
        }
    }

    void Schedule::execute_system(SystemDescriptor& system, const TimeStep& timestep)
    {
        if (system.execute_fn)
        {
            system.execute_fn();
        }
    }

    SystemDescriptor* Schedule::get_system(const std::string& name)
    {
        auto it = m_name_to_id.find(name);
        if (it != m_name_to_id.end())
        {
            return &m_systems[it->second];
        }
        return nullptr;
    }

    SystemDescriptor* Schedule::get_system_by_id(size_t id)
    {
        auto it = m_systems.find(id);
        if (it != m_systems.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    const SystemDescriptor* Schedule::get_system_by_id(size_t id) const
    {
        auto it = m_systems.find(id);
        if (it != m_systems.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    bool Schedule::has_system(const std::string& name) const
    {
        return m_name_to_id.find(name) != m_name_to_id.end();
    }

    bool Schedule::set_system_enabled(const std::string& name, bool enabled)
    {
        auto* descriptor = get_system(name);
        if (descriptor)
        {
            descriptor->enabled = enabled;
            return true;
        }
        return false;
    }

    bool Schedule::remove_system(const std::string& name)
    {
        auto it = m_name_to_id.find(name);
        if (it == m_name_to_id.end())
        {
            return false;
        }

        size_t id = it->second;
        m_systems.erase(id);
        m_id_to_name.erase(id);
        m_name_to_id.erase(it);

        m_is_built = false;
        return true;
    }

    void Schedule::clear()
    {
        m_systems.clear();
        m_name_to_id.clear();
        m_id_to_name.clear();
        m_graph->clear();
        m_next_system_id = 1;
        m_is_built = false;
    }

    const std::vector<size_t>& Schedule::get_execution_order() const
    {
        return m_graph->get_execution_order();
    }

    size_t Schedule::get_system_id(const std::string& name) const
    {
        auto it = m_name_to_id.find(name);
        return (it != m_name_to_id.end()) ? it->second : 0;
    }

    const std::string& Schedule::get_system_name(size_t id) const
    {
        static const std::string empty;
        auto it = m_id_to_name.find(id);
        return (it != m_id_to_name.end()) ? it->second : empty;
    }

    void Schedule::add_plugin(std::shared_ptr<Plugin> plugin)
    {
        if (plugin)
        {
            m_plugins.push_back(plugin);
            plugin->build(*this);
            DS_LOG_INFO("Added plugin: {}", plugin->name());
        }
    }

    void Schedule::set_label_enabled(const std::string& label, bool enabled)
    {
        for (auto& [id, descriptor] : m_systems)
        {
            for (const auto& l : descriptor.labels)
            {
                if (l == label)
                {
                    descriptor.enabled = enabled;
                }
            }
        }
    }

    std::vector<SystemDescriptor*> Schedule::get_systems_with_label(const std::string& label)
    {
        std::vector<SystemDescriptor*> result;
        for (auto& [id, descriptor] : m_systems)
        {
            for (const auto& l : descriptor.labels)
            {
                if (l == label)
                {
                    result.push_back(&descriptor);
                    break;
                }
            }
        }
        return result;
    }

    void Schedule::execute_stage(SystemStage stage, const TimeStep& timestep)
    {
        if (!m_is_built)
        {
            if (!build())
            {
                DS_LOG_ERROR("Cannot execute schedule: build failed");
                return;
            }
        }

        // Update WorldContext with current frame data
        m_world_context->update(
            m_scene,
            static_cast<void*>(&m_scene->get_registry()),
            const_cast<TimeStep*>(&timestep)
        );

        // Execute systems in the specified stage
        const auto& order = m_graph->get_execution_order();
        for (size_t sys_id : order)
        {
            auto& descriptor = m_systems[sys_id];
            if (descriptor.enabled && descriptor.stage == stage)
            {
                // Check run conditions
                bool should_run = true;
                for (const auto& condition : descriptor.conditions)
                {
                    if (!condition->evaluate(m_world_context.get()))
                    {
                        should_run = false;
                        break;
                    }
                }

                if (should_run)
                {
                    execute_system(descriptor, timestep);
                }
            }
        }
    }

    void Schedule::execute_startup_stages(const TimeStep& timestep)
    {
        if (m_has_executed_startup)
        {
            return;
        }

        execute_stage(SystemStage::PreStartup, timestep);
        execute_stage(SystemStage::Startup, timestep);
        execute_stage(SystemStage::PostStartup, timestep);

        m_has_executed_startup = true;
        DS_LOG_INFO("Startup stages executed");
    }

    // Helper methods for SystemConfig
    void Schedule::add_label_to_system(const std::string& system_name, const std::string& label)
    {
        auto* descriptor = get_system(system_name);
        if (descriptor)
        {
            descriptor->labels.push_back(label);
        }
    }

    void Schedule::set_system_stage(const std::string& system_name, SystemStage stage)
    {
        auto* descriptor = get_system(system_name);
        if (descriptor)
        {
            descriptor->stage = stage;
            m_is_built = false;  // Rebuild needed as stage affects order
        }
    }

    void Schedule::add_condition_to_system(const std::string& system_name, std::shared_ptr<ISystemCondition> condition)
    {
        auto* descriptor = get_system(system_name);
        if (descriptor && condition)
        {
            descriptor->conditions.push_back(condition);
        }
    }

} // namespace diverse::schedule
