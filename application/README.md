# Application layer

| Folder | Role |
|--------|------|
| `editor/` | Current ImGui-based editor (`divshot`) |
| `editor-api/` | Stable C ABI + `divshot-engine` sidecar |
| `editor-tauri/` | Tauri 2 + React editor shell |
| `diverseshot-cli/` | Headless / training CLI |

New editor work should go through `editor-api` so both ImGui and Tauri hosts can share engine logic.
