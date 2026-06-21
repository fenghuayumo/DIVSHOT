#pragma once

#include "schedule.h"
#include "system_condition.h"
#include "query.h"
#include "res.h"

#include <memory>

// Forward declarations
namespace diverse
{
    class Scene;
    struct TimeStep;
}

namespace diverse::schedule
{

    /**
     * @brief Core systems plugin for rendering and transform updates.
     *
     * This plugin provides essential systems for:
     * - Transform hierarchy updates
     * - Camera controller updates
     * - Render preparation
     */
    class CoreSystemsPlugin : public Plugin
    {
    public:
        /**
         * @brief Build the core systems into the schedule
         */
        void build(Schedule& schedule) override;

        /**
         * @brief Get the plugin name
         */
        const char* name() const override { return "CoreSystems"; }

        /**
         * @brief Configure which systems to enable
         */
        void enable_transform_system(bool enable = true) { m_transform_enabled = enable; }
        void enable_camera_system(bool enable = true) { m_camera_enabled = enable; }
        void enable_render_prepare(bool enable = true) { m_render_prepare_enabled = enable; }

    private:
        bool m_transform_enabled = true;
        bool m_camera_enabled = true;
        bool m_render_prepare_enabled = true;
    };

    /**
     * @brief Physics systems plugin.
     *
     * Provides systems for physics simulation.
     */
    class PhysicsPlugin : public Plugin
    {
    public:
        void build(Schedule& schedule) override;
        const char* name() const override { return "Physics"; }
    };

    /**
     * @brief Rendering systems plugin.
     *
     * Provides systems for render preparation and culling.
     */
    class RenderingPlugin : public Plugin
    {
    public:
        void build(Schedule& schedule) override;
        const char* name() const override { return "Rendering"; }
    };

} // namespace diverse::schedule
