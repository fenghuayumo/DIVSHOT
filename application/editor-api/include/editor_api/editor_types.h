#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Editor lifecycle / viewport flags */
typedef enum EditorApiFlags
{
    EditorApiFlag_None         = 0,
    EditorApiFlag_Headless     = 1u << 0, /**< No native viewport window */
    EditorApiFlag_EnableIpc    = 1u << 1, /**< Start JSON-RPC IPC server */
    EditorApiFlag_DisableImGui = 1u << 2  /**< Sidecar mode: skip ImGui shell */
} EditorApiFlags;

/** Engine initialization parameters passed from Tauri sidecar or host process */
typedef struct EditorInitParams
{
    const char* project_root;
    const char* render_api; /**< e.g. "Vulkan" */
    int         width;
    int         height;
    uint32_t    flags;
} EditorInitParams;

/** Native viewport placement (screen coordinates, logical pixels) */
typedef struct EditorViewportRect
{
    int x;
    int y;
    int width;
    int height;
} EditorViewportRect;

/** 3D transform payload for inspector / gizmo commands */
typedef struct EditorTransform
{
    float position[3];
    float rotation[3]; /**< Euler degrees */
    float scale[3];
} EditorTransform;

#ifdef __cplusplus
}
#endif
