#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace diverse
{
    class TimeStep;
    class Scene;
}

namespace diverse::schedule
{

    class WorldContext;

    /**
     * @brief Base class for system run conditions.
     *
     * Run conditions determine whether a system should execute based on runtime state.
     */
    class ISystemCondition
    {
    public:
        virtual ~ISystemCondition() = default;

        /**
         * @brief Check if the condition is met
         *
         * @param context The world context for this frame
         * @return true if the system should run, false otherwise
         */
        virtual bool evaluate(WorldContext* context) = 0;
    };

    /**
     * @brief Function-based run condition.
     */
    class FunctionCondition : public ISystemCondition
    {
    public:
        using ConditionFn = std::function<bool(WorldContext*)>;

        explicit FunctionCondition(ConditionFn fn)
            : m_function(std::move(fn))
        {
        }

        bool evaluate(WorldContext* context) override
        {
            return m_function(context);
        }

    private:
        ConditionFn m_function;
    };

    /**
     * @brief Label for grouping systems.
     *
     * Labels allow systems to be grouped and controlled together.
     * Systems can be enabled/disabled by label at runtime.
     */
    struct SystemLabel
    {
        std::string name;

        explicit SystemLabel(const std::string& label_name)
            : name(label_name)
        {
        }
    };

    /**
     * @brief Criteria for system execution.
     *
     * Systems can be configured to run based on criteria such as
     * having specific resources or components available.
     */
    enum class SystemCriteria
    {
        Always,           // System always runs
        HasResource,      // Runs if resource is available
        HasComponents     // Runs if entities with components exist
    };

    /**
     * @brief Stages for organizing system execution order.
     *
     * Systems can be assigned to stages for better organization.
     * Stages execute in order: PreStartup -> Startup -> PostStartup
     *                       -> PreUpdate -> Update -> PostUpdate
     *                       -> PreRender -> Render -> PostRender
     */
    enum class SystemStage
    {
        // Startup stages (run once when scene initializes)
        PreStartup,
        Startup,
        PostStartup,

        // Update stages (run every frame)
        PreUpdate,
        Update,
        PostUpdate,

        // Render stages (run every frame before rendering)
        PreRender,
        Render,
        PostRender,

        // Last stage (for cleanup)
        Last
    };

    /**
     * @brief Convert stage enum to string for logging
     */
    inline const char* stage_to_string(SystemStage stage)
    {
        switch (stage)
        {
            case SystemStage::PreStartup: return "PreStartup";
            case SystemStage::Startup: return "Startup";
            case SystemStage::PostStartup: return "PostStartup";
            case SystemStage::PreUpdate: return "PreUpdate";
            case SystemStage::Update: return "Update";
            case SystemStage::PostUpdate: return "PostUpdate";
            case SystemStage::PreRender: return "PreRender";
            case SystemStage::Render: return "Render";
            case SystemStage::PostRender: return "PostRender";
            case SystemStage::Last: return "Last";
            default: return "Unknown";
        }
    }

    /**
     * @brief Compare stages for ordering
     */
    inline bool stage_precedes(SystemStage a, SystemStage b)
    {
        return static_cast<int>(a) < static_cast<int>(b);
    }

} // namespace diverse::schedule
