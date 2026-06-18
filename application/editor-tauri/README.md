# editor-tauri

Tauri 2 shell for the next-generation DIVSHOT editor.

## Architecture

```
editor-tauri/          Web UI (React) + Tauri commands (Rust)
    ↕ invoke
src-tauri/             Rust bridge → sidecar IPC (Phase 1)
    ↕ spawn externalBin
../editor-api/         divshot-engine + editor_api C ABI
    ↕
diverse_base           Scene / ECS / Vulkan renderer
```

## Layout (matches `rfs/web-ui`)

```
Toolbar (menubar + icon toolbar)
DockLayout
  ├── SceneView (75%, transparent viewport + overlay)
  └── right column (25%)
        ├── Hierarchy
        └── Inspector | Post Process (tabs)
status-bar (training progress mock)
```

Styles are copied from `rfs/web-ui/src/styles/global.css` (VS Code dark theme).

## Phase 0 status

- Visual shell only — panels use static mock data
- Dock system supports split resize + tab switching (from rfs)
- No Tauri command / engine IPC wiring in UI yet
