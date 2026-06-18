#include <editor_api/editor_api.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>

namespace
{
    struct EditorApiState
    {
        std::mutex mutex;
        bool initialized = false;
        std::string last_error;
        std::string scratch;
        uint64_t selected_entity = 0;
        EditorViewportRect viewport_rect{0, 0, 1280, 720};
        uintptr_t parent_handle = 0;
    };

    EditorApiState& state()
    {
        static EditorApiState s;
        return s;
    }

    const char* store_scratch(std::string value)
    {
        auto& s = state();
        std::lock_guard lock(s.mutex);
        s.scratch = std::move(value);
        return s.scratch.c_str();
    }

    void set_error(std::string message)
    {
        auto& s = state();
        std::lock_guard lock(s.mutex);
        s.last_error = std::move(message);
    }

    int require_initialized()
    {
        auto& s = state();
        std::lock_guard lock(s.mutex);
        if (!s.initialized)
        {
            s.last_error = "editor_initialize() was not called";
            return 0;
        }
        return 1;
    }
}

extern "C" {

int editor_initialize(const EditorInitParams* params)
{
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (s.initialized)
    {
        s.last_error = "editor already initialized";
        return 0;
    }

    if (!params)
    {
        s.last_error = "EditorInitParams is null";
        return 0;
    }

    if (params->width > 0 && params->height > 0)
    {
        s.viewport_rect.width = params->width;
        s.viewport_rect.height = params->height;
    }

    s.initialized = true;
    s.last_error.clear();
    return 1;
}

void editor_shutdown()
{
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.initialized = false;
    s.selected_entity = 0;
    s.scratch.clear();
    s.last_error.clear();
}

int editor_tick(float /*delta_seconds*/)
{
    if (!require_initialized())
        return 0;

    // Phase 0 stub: engine update/render will be wired here.
    return 1;
}

const char* editor_get_version()
{
    return store_scratch("0.1.0-stub");
}

const char* editor_get_last_error()
{
    auto& s = state();
    std::lock_guard lock(s.mutex);
    return s.last_error.c_str();
}

const char* editor_get_scene_hierarchy_json()
{
    if (!require_initialized())
        return store_scratch("{\"error\":\"not initialized\"}");

    return store_scratch(R"({"entities":[{"id":1,"name":"Root","children":[]}],"selected":0})");
}

void editor_select_entity(uint64_t entity_id)
{
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.selected_entity = entity_id;
}

uint64_t editor_get_selected_entity()
{
    auto& s = state();
    std::lock_guard lock(s.mutex);
    return s.selected_entity;
}

int editor_get_entity_transform(uint64_t entity_id, EditorTransform* out_transform)
{
    if (!require_initialized() || !out_transform)
        return 0;

    if (entity_id == 0)
        return 0;

    std::memset(out_transform, 0, sizeof(EditorTransform));
    out_transform->scale[0] = out_transform->scale[1] = out_transform->scale[2] = 1.0f;
    return 1;
}

int editor_set_entity_transform(uint64_t entity_id, const EditorTransform* transform)
{
    if (!require_initialized() || !transform || entity_id == 0)
        return 0;

    // Phase 0 stub: accept command, real ECS write comes later.
    return 1;
}

int editor_set_viewport_parent_handle(uintptr_t parent_window_handle)
{
    if (!require_initialized())
        return 0;

    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.parent_handle = parent_window_handle;
    return 1;
}

int editor_set_viewport_rect(const EditorViewportRect* rect)
{
    if (!require_initialized() || !rect)
        return 0;

    if (rect->width <= 0 || rect->height <= 0)
    {
        set_error("invalid viewport size");
        return 0;
    }

    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.viewport_rect = *rect;
    return 1;
}

int editor_viewport_focused()
{
    if (!require_initialized())
        return 0;

    return 0;
}

int editor_new_project(const char* project_root)
{
    if (!project_root || project_root[0] == '\0')
    {
        set_error("project_root is empty");
        return 0;
    }

    if (!require_initialized())
        return 0;

    return 1;
}

int editor_open_project(const char* project_path)
{
    if (!project_path || project_path[0] == '\0')
    {
        set_error("project_path is empty");
        return 0;
    }

    if (!require_initialized())
        return 0;

    return 1;
}

int editor_save_project()
{
    return require_initialized();
}

int editor_ipc_start(uint16_t /*port*/)
{
    if (!require_initialized())
        return 0;

    // Wired in ipc_server.cpp (Phase 0 placeholder).
    return 1;
}

void editor_ipc_stop()
{
}

int editor_ipc_poll()
{
    if (!require_initialized())
        return 0;

    return 0;
}

} // extern "C"
