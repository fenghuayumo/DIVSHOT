#pragma once

/**
 * Stable C ABI between DIVSHOT engine and external hosts (Tauri sidecar, tests, automation).
 * All string returns are owned by the library until the next call on the same thread,
 * or until editor_shutdown().
 */

#include "editor_types.h"

#include <stddef.h>
#include <stdint.h>

#if defined(EDITOR_API_SHARED) && defined(_WIN32)
#  if defined(EDITOR_API_EXPORT)
#    define EDITOR_API __declspec(dllexport)
#  else
#    define EDITOR_API __declspec(dllimport)
#  endif
#else
#  define EDITOR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle ---- */

EDITOR_API int  editor_initialize(const EditorInitParams* params);
EDITOR_API void editor_shutdown();
EDITOR_API int  editor_tick(float delta_seconds);
EDITOR_API const char* editor_get_version();
EDITOR_API const char* editor_get_last_error();

/* ---- Scene / selection ---- */

EDITOR_API const char* editor_get_scene_hierarchy_json();
EDITOR_API void        editor_select_entity(uint64_t entity_id);
EDITOR_API uint64_t    editor_get_selected_entity();
EDITOR_API int         editor_get_entity_transform(uint64_t entity_id, EditorTransform* out_transform);
EDITOR_API int         editor_set_entity_transform(uint64_t entity_id, const EditorTransform* transform);

/* ---- Viewport (native child window) ---- */

EDITOR_API int  editor_set_viewport_parent_handle(uintptr_t parent_window_handle);
EDITOR_API int  editor_set_viewport_rect(const EditorViewportRect* rect);
EDITOR_API int  editor_viewport_focused();

/* ---- Project I/O ---- */

EDITOR_API int  editor_new_project(const char* project_root);
EDITOR_API int  editor_open_project(const char* project_path);
EDITOR_API int  editor_save_project();

/* ---- IPC (JSON-RPC over TCP, localhost) ---- */

EDITOR_API int  editor_ipc_start(uint16_t port);
EDITOR_API void editor_ipc_stop();
EDITOR_API int  editor_ipc_poll();

#ifdef __cplusplus
}
#endif
