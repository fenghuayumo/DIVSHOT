# editor-api

C ABI boundary between DIVSHOT engine and external hosts (Tauri sidecar, tests, automation).

## Layout

```
editor-api/
  include/editor_api/   Public headers (stable C ABI)
  source/               Implementation + IPC + sidecar entry
```

## Targets

| Target | Type | Purpose |
|--------|------|---------|
| `editor_api` | SHARED library | Stable C API (`editor_api.h`) |
| `divshot-engine` | executable | Tauri sidecar host loop + IPC |

## Consumers

- `application/editor/` — existing ImGui editor (will call into editor-api over time)
- `application/editor-tauri/` — Tauri shell; spawns `divshot-engine` as `externalBin`

## Build (CMake)

Built automatically when `application/CMakeLists.txt` is configured.

## Phase 0 status

All API functions are stubbed. Next steps:

1. Wire `editor_tick` to `Application::frame()` without ImGui shell
2. Implement `editor_get_scene_hierarchy_json` from `entt` registry
3. Complete `IpcServer` JSON-RPC dispatch
4. Native viewport: `editor_set_viewport_parent_handle` + Vulkan child surface
