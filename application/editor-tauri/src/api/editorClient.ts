import { invoke } from "@tauri-apps/api/core";
import type { EditorTransform, EngineStatus, SceneHierarchy, ViewportRect } from "./types";

/** Thin wrapper over Tauri commands → Rust → divshot-engine IPC */
export const editorClient = {
  startEngine(projectRoot?: string): Promise<EngineStatus> {
    return invoke<EngineStatus>("engine_start", { projectRoot: projectRoot ?? "." });
  },

  stopEngine(): Promise<void> {
    return invoke("engine_stop");
  },

  getStatus(): Promise<EngineStatus> {
    return invoke<EngineStatus>("engine_status");
  },

  getHierarchy(): Promise<SceneHierarchy> {
    return invoke<SceneHierarchy>("scene_get_hierarchy");
  },

  selectEntity(entityId: number): Promise<void> {
    return invoke("scene_select_entity", { entityId });
  },

  getTransform(entityId: number): Promise<EditorTransform> {
    return invoke<EditorTransform>("scene_get_transform", { entityId });
  },

  setTransform(entityId: number, transform: EditorTransform): Promise<void> {
    return invoke("scene_set_transform", { entityId, transform });
  },

  setViewportRect(rect: ViewportRect): Promise<void> {
    return invoke("viewport_set_rect", { rect });
  },
};
